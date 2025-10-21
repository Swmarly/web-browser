// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

namespace {

// Parameters for the refresh quota. We currently allow 2 refreshes in 5
// minutes. This allows sites to refresh every 5 minutes with some error
// tolerance (e.g. a failed refresh or user cookie clearing).
constexpr size_t kRefreshQuota = 2;
constexpr base::TimeDelta kRefreshQuotaInterval = base::Minutes(5);

bool SessionMatchesFilter(
    const SchemefulSite& site,
    const Session& session,
    std::optional<base::Time> created_after_time,
    std::optional<base::Time> created_before_time,
    base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
        origin_and_site_matcher) {
  if (created_before_time && *created_before_time < session.creation_date()) {
    return false;
  }

  if (created_after_time && *created_after_time > session.creation_date()) {
    return false;
  }

  if (!origin_and_site_matcher.is_null() &&
      !origin_and_site_matcher.Run(session.origin(), site)) {
    return false;
  }

  return true;
}

class DebugHeaderBuilder {
 public:
  void AddSkippedSession(SessionKey key, SessionService::RefreshResult result) {
    structured_headers::Item item;
    switch (result) {
      case SessionService::RefreshResult::kRefreshed:
      case SessionService::RefreshResult::kFatalError:
        return;
      case SessionService::RefreshResult::kInitializedService:
        NOTREACHED();
      case SessionService::RefreshResult::kUnreachable:
        item = structured_headers::Item("unreachable",
                                        structured_headers::Item::kTokenType);
        break;
      case SessionService::RefreshResult::kServerError:
        item = structured_headers::Item("server_error",
                                        structured_headers::Item::kTokenType);
        break;
      case SessionService::RefreshResult::kQuotaExceeded:
        item = structured_headers::Item("quota_exceeded",
                                        structured_headers::Item::kTokenType);
        break;
    }

    structured_headers::Parameters params = {
        {"session_identifier", structured_headers::Item(key.id.value())}};
    skipped_sessions_.emplace_back(std::move(item), std::move(params));
  }

  std::optional<std::string> Build() {
    if (skipped_sessions_.empty()) {
      return std::nullopt;
    }

    return structured_headers::SerializeList(std::move(skipped_sessions_));
  }

 private:
  structured_headers::List skipped_sessions_;
};

bool IsProactiveRefreshCandidate(
    Session& existing_session,
    const Session& new_session,
    const CookieAndLineAccessResultList& maybe_stored_cookies) {
  // Get the shortest lifetime of a bound cookie set by the current
  // refresh request. This assumes:
  // 1. The current refresh sets all bound cookies
  // 2. The proactive refresh would have set the same lifetimes
  // These assumptions are good enough for histogram logging, but likely
  // not true for all sites.
  base::Time current_time = base::Time::Now();
  base::TimeDelta minimum_lifetime = base::TimeDelta::Max();
  for (const CookieCraving& cookie_craving : new_session.cookies()) {
    for (const CookieAndLineWithAccessResult& cookie_and_line :
         maybe_stored_cookies) {
      if (cookie_and_line.cookie.has_value() &&
          cookie_craving.IsSatisfiedBy(cookie_and_line.cookie.value())) {
        minimum_lifetime =
            std::min(minimum_lifetime,
                     cookie_and_line.cookie->ExpiryDate() - current_time);
      }
    }
  }

  base::UmaHistogramLongTimes100(
      "Net.DeviceBoundSessions.MinimumBoundCookieLifetime", minimum_lifetime);

  std::optional<base::Time> last_proactive_refresh_opportunity =
      existing_session.TakeLastProactiveRefreshOpportunity();

  if (!last_proactive_refresh_opportunity.has_value()) {
    return false;
  }

  return minimum_lifetime >= current_time - *last_proactive_refresh_opportunity;
}

}  // namespace

DeferredURLRequest::DeferredURLRequest(
    SessionService::RefreshCompleteCallback callback)
    : callback(std::move(callback)) {}

DeferredURLRequest::DeferredURLRequest(DeferredURLRequest&& other) noexcept =
    default;

DeferredURLRequest& DeferredURLRequest::operator=(
    DeferredURLRequest&& other) noexcept = default;

DeferredURLRequest::~DeferredURLRequest() = default;

SessionServiceImpl::SessionServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* request_context,
    SessionStore* store)
    : pending_initialization_(!!store),
      key_service_(key_service),
      context_(request_context),
      session_store_(store) {
  ignore_refresh_quota_ = !features::kDeviceBoundSessionsRefreshQuota.Get();
  CHECK(context_);
}

SessionServiceImpl::~SessionServiceImpl() = default;

void SessionServiceImpl::LoadSessionsAsync() {
  if (!session_store_) {
    return;
  }
  session_store_->LoadSessions(base::BindOnce(
      &SessionServiceImpl::OnLoadSessionsComplete, weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::RegisterBoundSession(
    OnAccessCallback on_access_callback,
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info,
    const NetLogWithSource& net_log,
    const std::optional<url::Origin>& original_request_initiator) {
  Session* federated_provider_session = nullptr;
  bool is_google_subdomain_for_histograms = IsSubdomainOf(
      registration_params.registration_endpoint().host(), "google.com");
  if (registration_params.provider_session_id().has_value()) {
    if (!base::FeatureList::IsEnabled(
            features::kDeviceBoundSessionsFederatedRegistration)) {
      // Simply ignore headers with a provider_session_id if the flag
      // isn't enabled.
      return;
    }

    base::expected<Session*, SessionError> provider_session_or_error =
        GetFederatedProviderSessionIfValid(registration_params);
    if (!provider_session_or_error.has_value()) {
      OnRegistrationComplete(
          std::move(on_access_callback), is_google_subdomain_for_histograms,
          /*fetcher=*/nullptr,
          RegistrationResult(std::move(provider_session_or_error.error())));
      return;
    }

    federated_provider_session = provider_session_or_error.value();
  }

  net::NetLogSource net_log_source_for_registration = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  net_log.AddEventReferencingSource(
      net::NetLogEventType::DBSC_REGISTRATION_REQUEST,
      net_log_source_for_registration);

  const auto supported_algos = registration_params.supported_algos();
  std::optional<GURL> provider_url = registration_params.provider_url();
  RegistrationRequestParam request_params =
      RegistrationRequestParam::CreateForRegistration(
          std::move(registration_params));
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          request_params, *this, key_service_.get(), context_.get(),
          isolation_info, net_log_source_for_registration,
          original_request_initiator);
  RegistrationFetcher* fetcher_raw = fetcher.get();
  registration_fetchers_.insert(std::move(fetcher));

  auto callback = base::BindOnce(
      &SessionServiceImpl::OnRegistrationComplete, weak_factory_.GetWeakPtr(),
      std::move(on_access_callback), is_google_subdomain_for_histograms);
  if (federated_provider_session) {
    fetcher_raw->StartFetchWithFederatedKey(
        request_params, *federated_provider_session->unexportable_key_id(),
        *provider_url, std::move(callback));
    // `fetcher_raw` may be deleted.
  } else {
    fetcher_raw->StartCreateTokenAndFetch(request_params, supported_algos,
                                          std::move(callback));
    // `fetcher_raw` may be deleted.
  }
}

base::expected<Session*, SessionError>
SessionServiceImpl::GetFederatedProviderSessionIfValid(
    const RegistrationFetcherParam& registration_params) {
  // This is a federated session registration.
  GURL provider_url = *registration_params.provider_url();
  if (!provider_url.is_valid() || url::Origin::Create(provider_url).opaque()) {
    return base::unexpected(
        SessionError(SessionError::kInvalidFederatedSessionUrl));
  }

  SessionKey provider_key{SchemefulSite(provider_url),
                          *registration_params.provider_session_id()};
  Session* provider_session = GetSession(provider_key);

  if (!provider_session) {
    // Provider session not found, fail the registration.
    return base::unexpected(
        SessionError(SessionError::kInvalidFederatedSession));
  }

  if (url::Origin::Create(provider_url) != provider_session->origin()) {
    return base::unexpected(
        SessionError(SessionError::kInvalidFederatedSession));
  }

  unexportable_keys::ServiceErrorOr<
      crypto::SignatureVerifier::SignatureAlgorithm>
      algorithm =
          key_service_->GetAlgorithm(*provider_session->unexportable_key_id());
  if (!algorithm.has_value()) {
    return base::unexpected(SessionError(SessionError::kInvalidFederatedKey));
  }

  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> pub_key =
      key_service_->GetSubjectPublicKeyInfo(
          *provider_session->unexportable_key_id());
  if (!pub_key.has_value()) {
    return base::unexpected(SessionError(SessionError::kInvalidFederatedKey));
  }

  std::string thumbprint = CreateJwkThumbprint(*algorithm, *pub_key);
  if (thumbprint != *registration_params.provider_key()) {
    return base::unexpected(
        SessionError(SessionError::kFederatedKeyThumbprintMismatch));
  }

  return provider_session;
}

SessionServiceImpl::Observer::Observer(
    const GURL& url,
    base::RepeatingCallback<void(const SessionAccess&)> callback)
    : url(url), callback(callback) {}
SessionServiceImpl::Observer::~Observer() = default;

void SessionServiceImpl::OnLoadSessionsComplete(
    SessionStore::SessionsMap sessions) {
  unpartitioned_sessions_.merge(sessions);
  pending_initialization_ = false;

  std::vector<base::OnceClosure> queued_operations =
      std::move(queued_operations_);
  for (base::OnceClosure& closure : queued_operations) {
    std::move(closure).Run();
  }

  base::UmaHistogramCounts1000(
      "Net.DeviceBoundSessions.RequestsDeferredForInitialization",
      requests_before_initialization_);
}

void SessionServiceImpl::OnRegistrationComplete(
    OnAccessCallback on_access_callback,
    bool is_google_subdomain_for_histograms,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  if (is_google_subdomain_for_histograms) {
    base::UmaHistogramBoolean(
        "Net.DeviceBoundSessions.GoogleRegistrationIsFromStandard", true);
  }
  SessionError::ErrorType result = OnRegistrationCompleteInternal(
      std::move(on_access_callback), fetcher, std::move(registration_result));
  base::UmaHistogramEnumeration("Net.DeviceBoundSessions.RegistrationResult",
                                result);
}

std::ranges::subrange<SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  const auto now = base::Time::Now();
  // Session keys are sorted by site, then identifier. So the first
  // element not less than (`site`, "") is the first session for this
  // site.
  auto it =
      unpartitioned_sessions_.lower_bound(SessionKey{site, Session::Id("")});
  while (it != unpartitioned_sessions_.end() && it->first.site == site) {
    auto curit = it;
    ++it;

    if (now >= curit->second->expiry_date()) {
      // Since this deletion is not due to a request, we do not need to
      // provide a per-request callback here.
      DeleteSessionAndNotifyInternal(DeletionReason::kExpired, curit,
                                     base::NullCallback());
    } else {
      curit->second->RecordAccess();
    }
  }

  return std::ranges::subrange<SessionsMap::iterator>(
      unpartitioned_sessions_.lower_bound(SessionKey{site, Session::Id("")}),
      it);
}

std::optional<SessionService::DeferralParams> SessionServiceImpl::ShouldDefer(
    URLRequest* request,
    HttpRequestHeaders* extra_headers,
    const FirstPartySetMetadata& first_party_set_metadata) {
  if (pending_initialization_) {
    return DeferralParams();
  }

  if (request->device_bound_session_usage() < SessionUsage::kNoUsage) {
    request->set_device_bound_session_usage(SessionUsage::kNoUsage);
  }

  SchemefulSite site(request->url());
  DebugHeaderBuilder debug_header_builder;
  const base::flat_map<SessionKey, RefreshResult>& previous_deferrals =
      request->device_bound_session_deferrals();
  for (const auto& [_, session] : GetSessionsForSite(site)) {
    if (session->ShouldDeferRequest(request, first_party_set_metadata)) {
      SessionKey session_key{site, session->id()};
      auto previous_deferrals_it = previous_deferrals.find(session_key);
      if (previous_deferrals_it != previous_deferrals.end()) {
        debug_header_builder.AddSkippedSession(previous_deferrals_it->first,
                                               previous_deferrals_it->second);
        continue;
      }

      NotifySessionAccess(request->device_bound_session_access_callback(),
                          SessionAccess::AccessType::kUpdate, session_key,
                          *session);
      return DeferralParams(session->id());
    }
  }

  std::optional<std::string> debug_header = debug_header_builder.Build();
  if (debug_header.has_value()) {
    extra_headers->SetHeader("Secure-Session-Skipped", *debug_header);
  }

  return std::nullopt;
}

void SessionServiceImpl::DeferRequestForRefresh(
    URLRequest* request,
    DeferralParams deferral,
    RefreshCompleteCallback callback) {
  CHECK(callback);
  CHECK(request);

  if (deferral.is_pending_initialization) {
    CHECK(pending_initialization_);
    requests_before_initialization_++;
    // Due to the need to recompute `first_party_set_metadata`, we always
    // restart the request after initialization completes.
    queued_operations_.push_back(base::BindOnce(
        std::move(callback), RefreshResult::kInitializedService));
    return;
  }

  SessionKey session_key{SchemefulSite(request->url()), *deferral.session_id};
  // For the first deferring request, create a new vector and add the request.
  auto [it, inserted] = deferred_requests_.try_emplace(session_key.id);
  // Add the request callback to the deferred list.
  it->second.emplace_back(std::move(callback));

  auto* session = GetSession(session_key);
  CHECK(session, base::NotFatalUntil::M147);
  // TODO(crbug.com/417770933): Remove this block.
  if (!session) {
    // If we can't find the session, clear the `session_key` in the map
    // and continue all related requests. We can call this a fatal error
    // because the session has already been deleted.
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    return;
  }
  // Notify the request that it has been deferred for refreshed cookies.
  NotifySessionAccess(request->device_bound_session_access_callback(),
                      SessionAccess::AccessType::kUpdate, session_key,
                      *session);
  if (!inserted) {
    return;
  }

  if (RefreshQuotaExceeded(session_key.site)) {
    UnblockDeferredRequests(session_key, RefreshResult::kQuotaExceeded);
    return;
  }

  if (session->ShouldBackoff()) {
    UnblockDeferredRequests(session_key, RefreshResult::kUnreachable);
    return;
  }

  const Session::KeyIdOrError& key_id = session->unexportable_key_id();
  if (!key_id.has_value()) {
    if (key_id.error() == unexportable_keys::ServiceError::kKeyNotReady) {
      // Unwrap key and then try to refresh
      session_store_->RestoreSessionBindingKey(
          session_key,
          base::BindOnce(&SessionServiceImpl::OnSessionKeyRestored,
                         weak_factory_.GetWeakPtr(), request->GetWeakPtr(),
                         session_key,
                         request->device_bound_session_access_callback()));
    } else {
      UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
      DeleteSessionAndNotify(DeletionReason::kFailedToRestoreKey, session_key,
                             request->device_bound_session_access_callback());
    }

    return;
  }

  RefreshSessionInternal(request, session_key, session, *key_id);
}

void SessionServiceImpl::OnRefreshRequestCompletion(
    OnAccessCallback on_access_callback,
    SessionKey session_key,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  SessionError::ErrorType result = OnRefreshRequestCompletionInternal(
      std::move(on_access_callback), session_key, fetcher,
      std::move(registration_result));

  Session* session = GetSession(session_key);
  if (session) {
    session->InformOfRefreshResult(result);
  }

  base::UmaHistogramEnumeration("Net.DeviceBoundSessions.RefreshResult",
                                result);
}

// Continue or restart all deferred requests for the session and remove the
// session_id key in the map.
void SessionServiceImpl::UnblockDeferredRequests(
    const SessionKey& session_key,
    RefreshResult result,
    std::optional<bool> is_proactive_refresh_candidate,
    std::optional<base::TimeDelta> minimum_proactive_refresh_threshold) {
  auto it = deferred_requests_.find(session_key.id);
  if (it == deferred_requests_.end()) {
    return;
  }

  auto requests = std::move(it->second);
  deferred_requests_.erase(it);

  base::UmaHistogramCounts100("Net.DeviceBoundSessions.RequestDeferredCount",
                              requests.size());

  if (is_proactive_refresh_candidate.has_value() &&
      minimum_proactive_refresh_threshold.has_value()) {
    base::UmaHistogramLongTimes100(
        "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold",
        *minimum_proactive_refresh_threshold);
    if (*is_proactive_refresh_candidate) {
      base::UmaHistogramLongTimes100(
          "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold.Success",
          *minimum_proactive_refresh_threshold);
    } else {
      base::UmaHistogramLongTimes100(
          "Net.DeviceBoundSessions.MinimumProactiveRefreshThreshold.Failure",
          *minimum_proactive_refresh_threshold);
    }

    if (*is_proactive_refresh_candidate) {
      if (*minimum_proactive_refresh_threshold <= base::Seconds(30)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "ThirtySeconds",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.ThirtySeconds",
              request.timer.Elapsed());
        }
      }

      if (*minimum_proactive_refresh_threshold <= base::Minutes(1)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "OneMinute",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.OneMinute",
              request.timer.Elapsed());
        }
      }

      if (*minimum_proactive_refresh_threshold <= base::Minutes(2)) {
        base::UmaHistogramCounts100(
            "Net.DeviceBoundSessions.ProactiveRefreshCandidateDeferredCount."
            "TwoMinutes",
            requests.size());
        for (auto& request : requests) {
          base::UmaHistogramTimes(
              "Net.DeviceBoundSessions."
              "ProactiveRefreshCandidateRequestDeferredDuration.TwoMinutes",
              request.timer.Elapsed());
        }
      }
    }
  }

  for (auto& request : requests) {
    base::UmaHistogramTimes("Net.DeviceBoundSessions.RequestDeferredDuration",
                            request.timer.Elapsed());
    base::UmaHistogramEnumeration("Net.DeviceBoundSessions.DeferralResult",
                                  result);
    if (request.timer.Elapsed() <= base::Milliseconds(1)) {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.DeferralResult.Instant", result);
    } else {
      base::UmaHistogramEnumeration(
          "Net.DeviceBoundSessions.DeferralResult.Slow", result);
    }
    std::move(request.callback).Run(result);
  }
}

void SessionServiceImpl::SetChallengeForBoundSession(
    OnAccessCallback on_access_callback,
    const URLRequest& request,
    const FirstPartySetMetadata& first_party_set_metadata,
    const SessionChallengeParam& param) {
  if (!param.session_id()) {
    return;
  }

  SessionKey session_key{SchemefulSite(request.url()),
                         Session::Id(*param.session_id())};
  Session* session = GetSession(session_key);
  if (!session) {
    return;
  }

  if (features::kDeviceBoundSessionsOriginTrialFeedback.Get() &&
      !session->CanSetBoundCookie(request, first_party_set_metadata)) {
    return;
  }

  NotifySessionAccess(on_access_callback, SessionAccess::AccessType::kUpdate,
                      session_key, *session);
  session->set_cached_challenge(param.challenge());
}

void SessionServiceImpl::GetAllSessionsAsync(
    base::OnceCallback<void(const std::vector<SessionKey>&)> callback) {
  if (pending_initialization_) {
    queued_operations_.push_back(base::BindOnce(
        &SessionServiceImpl::GetAllSessionsAsync,
        // `base::Unretained` is safe because the callback is stored in
        // `queued_operations_`, which is owned by `this`.
        base::Unretained(this), std::move(callback)));
  } else {
    std::vector<SessionKey> sessions = base::ToVector(
        unpartitioned_sessions_, [](const auto& pair) { return pair.first; });
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(sessions)));
  }
}

void SessionServiceImpl::DeleteSessionAndNotify(
    DeletionReason reason,
    const SessionKey& session_key,
    SessionService::OnAccessCallback per_request_callback) {
  auto it = unpartitioned_sessions_.find(session_key);
  if (it == unpartitioned_sessions_.end()) {
    return;
  }

  DeleteSessionAndNotifyInternal(reason, it, per_request_callback);
}

const Session* SessionServiceImpl::GetSession(
    const SessionKey& session_key) const {
  auto it = unpartitioned_sessions_.find(session_key);
  if (it != unpartitioned_sessions_.end()) {
    return it->second.get();
  }
  return nullptr;
}

Session* SessionServiceImpl::GetSession(const SessionKey& session_key) {
  return const_cast<Session*>(std::as_const(*this).GetSession(session_key));
}

void SessionServiceImpl::AddSession(const SchemefulSite& site,
                                    std::unique_ptr<Session> session) {
  if (session_store_) {
    session_store_->SaveSession(site, *session);
  }

  unpartitioned_sessions_.emplace(SessionKey{site, session->id()},
                                  std::move(session));
}

void SessionServiceImpl::DeleteAllSessions(
    DeletionReason reason,
    std::optional<base::Time> created_after_time,
    std::optional<base::Time> created_before_time,
    base::RepeatingCallback<bool(const url::Origin&, const net::SchemefulSite&)>
        origin_and_site_matcher,
    base::OnceClosure completion_callback) {
  for (auto it = unpartitioned_sessions_.begin();
       it != unpartitioned_sessions_.end();) {
    auto curit = it;
    ++it;

    if (SessionMatchesFilter(curit->first.site, *curit->second,
                             created_after_time, created_before_time,
                             origin_and_site_matcher)) {
      DeleteSessionAndNotifyInternal(reason, curit, base::NullCallback());
    }
  }

  std::move(completion_callback).Run();
}

base::ScopedClosureRunner SessionServiceImpl::AddObserver(
    const GURL& url,
    base::RepeatingCallback<void(const SessionAccess&)> callback) {
  auto observer = std::make_unique<Observer>(url, callback);
  base::ScopedClosureRunner subscription(base::BindOnce(
      &SessionServiceImpl::RemoveObserver, weak_factory_.GetWeakPtr(),
      net::SchemefulSite(url), observer.get()));
  observers_by_site_[net::SchemefulSite(url)].insert(std::move(observer));
  return subscription;
}

void SessionServiceImpl::DeleteSessionAndNotifyInternal(
    DeletionReason reason,
    SessionServiceImpl::SessionsMap::iterator it,
    SessionService::OnAccessCallback per_request_callback) {
  base::UmaHistogramEnumeration("Net.DeviceBoundSessions.DeletionReason",
                                reason);

  if (session_store_) {
    session_store_->DeleteSession(it->first);
  }

  NotifySessionAccess(per_request_callback,
                      SessionAccess::AccessType::kTermination, it->first,
                      *it->second);

  unpartitioned_sessions_.erase(it);
}

void SessionServiceImpl::NotifySessionAccess(
    SessionService::OnAccessCallback per_request_callback,
    SessionAccess::AccessType access_type,
    const SessionKey& session_key,
    const Session& session) {
  SessionAccess access{access_type, session_key};

  if (access_type == SessionAccess::AccessType::kTermination) {
    access.cookies.reserve(session.cookies().size());
    for (const CookieCraving& cookie : session.cookies()) {
      access.cookies.push_back(cookie.Name());
    }
  }

  if (per_request_callback) {
    per_request_callback.Run(access);
  }

  auto observers_it = observers_by_site_.find(session_key.site);
  if (observers_it == observers_by_site_.end()) {
    return;
  }

  for (const auto& observer : observers_it->second) {
    if (session.IncludesUrl(observer->url)) {
      observer->callback.Run(access);
    }
  }
}

void SessionServiceImpl::RemoveObserver(net::SchemefulSite site,
                                        Observer* observer) {
  auto observers_it = observers_by_site_.find(site);
  if (observers_it == observers_by_site_.end()) {
    return;
  }

  ObserverSet& observers = observers_it->second;

  auto it = observers.find(observer);
  if (it == observers.end()) {
    return;
  }

  observers.erase(it);

  if (observers.empty()) {
    observers_by_site_.erase(observers_it);
  }
}

SessionError::ErrorType SessionServiceImpl::OnRegistrationCompleteInternal(
    OnAccessCallback on_access_callback,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  RemoveFetcher(fetcher);

  if (registration_result.is_error()) {
    // We failed to create a new session, so there's nothing to clean
    // up.
    return registration_result.error().type;
  } else if (registration_result.is_no_session_config_change()) {
    // No config changes is not allowed at registration.
    return SessionError::kInvalidConfigJson;
  }

  std::unique_ptr<Session> session = registration_result.TakeSession();
  CHECK(session);
  const SchemefulSite site(session->origin());
  NotifySessionAccess(on_access_callback, SessionAccess::AccessType::kCreation,
                      SessionKey{site, session->id()}, *session);
  AddSession(site, std::move(session));
  return SessionError::kSuccess;
}

SessionError::ErrorType SessionServiceImpl::OnRefreshRequestCompletionInternal(
    OnAccessCallback on_access_callback,
    const SessionKey& session_key,
    RegistrationFetcher* fetcher,
    RegistrationResult registration_result) {
  RemoveFetcher(fetcher);

  // If refresh succeeded:
  // 1. update the session by adding a new session, replacing the old one
  // 2. restart the deferred requests.
  if (registration_result.is_session()) {
    std::unique_ptr<Session> new_session = registration_result.TakeSession();
    CHECK(new_session);
    CHECK_EQ(new_session->id(), session_key.id);

    Session* existing_session = GetSession(session_key);
    CHECK(existing_session);
    bool is_proactive_refresh_candidate =
        IsProactiveRefreshCandidate(*existing_session, *new_session,
                                    registration_result.maybe_stored_cookies());

    SchemefulSite new_site(new_session->origin());
    AddSession(new_site, std::move(new_session));
    // The session has been refreshed, restart the request.
    UnblockDeferredRequests(
        session_key, RefreshResult::kRefreshed, is_proactive_refresh_candidate,
        existing_session
            ->TakeLastProactiveRefreshOpportunityMinimumCookieLifetime());
  } else if (registration_result.is_no_session_config_change()) {
    Session* existing_session = GetSession(session_key);
    CHECK(existing_session);
    bool is_proactive_refresh_candidate =
        IsProactiveRefreshCandidate(*existing_session, *existing_session,
                                    registration_result.maybe_stored_cookies());

    UnblockDeferredRequests(
        session_key, RefreshResult::kRefreshed, is_proactive_refresh_candidate,
        existing_session
            ->TakeLastProactiveRefreshOpportunityMinimumCookieLifetime());
  } else if (std::optional<DeletionReason> deletion_reason =
                 registration_result.error().GetDeletionReason();
             deletion_reason.has_value()) {
    DeleteSessionAndNotify(*deletion_reason, session_key, on_access_callback);
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
  } else {
    // Transient error, unblock the request without cookies.
    UnblockDeferredRequests(session_key,
                            registration_result.error().IsServerError()
                                ? RefreshResult::kServerError
                                : RefreshResult::kUnreachable);
  }

  return registration_result.is_error() ? registration_result.error().type
                                        : SessionError::kSuccess;
}

void SessionServiceImpl::OnSessionKeyRestored(
    base::WeakPtr<URLRequest> request,
    const SessionKey& session_key,
    OnAccessCallback on_access_callback,
    Session::KeyIdOrError key_id_or_error) {
  if (!request) {
    return;
  }

  if (!key_id_or_error.has_value()) {
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    DeleteSessionAndNotify(DeletionReason::kFailedToUnwrapKey, session_key,
                           on_access_callback);
    return;
  }

  auto* session = GetSession(session_key);
  if (!session) {
    UnblockDeferredRequests(session_key, RefreshResult::kFatalError);
    return;
  }

  session->set_unexportable_key_id(key_id_or_error);

  RefreshSessionInternal(request.get(), session_key, session, *key_id_or_error);
}

void SessionServiceImpl::RefreshSessionInternal(
    URLRequest* request,
    const SessionKey& session_key,
    Session* session,
    unexportable_keys::UnexportableKeyId key_id) {
  net::NetLogSource net_log_source_for_refresh = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  request->net_log().AddEventReferencingSource(
      net::NetLogEventType::DBSC_REFRESH_REQUEST, net_log_source_for_refresh);

  refresh_times_[session_key.site].push_back(base::TimeTicks::Now());

  auto registration_param =
      RegistrationRequestParam::CreateForRefresh(*session);

  auto callback = base::BindOnce(
      &SessionServiceImpl::OnRefreshRequestCompletion,
      weak_factory_.GetWeakPtr(),
      request->device_bound_session_access_callback(), session_key);
  std::unique_ptr<RegistrationFetcher> fetcher =
      RegistrationFetcher::CreateFetcher(
          registration_param, *this, key_service_.get(), context_.get(),
          request->isolation_info(), net_log_source_for_refresh,
          request->initiator());
  RegistrationFetcher* fetcher_raw = fetcher.get();
  registration_fetchers_.insert(std::move(fetcher));
  fetcher_raw->StartFetchWithExistingKey(registration_param, key_id,
                                         std::move(callback));
  // `fetcher_raw` may be deleted.
}

bool SessionServiceImpl::RefreshQuotaExceeded(const SchemefulSite& site) {
  if (ignore_refresh_quota_) {
    return false;
  }

  auto it = refresh_times_.find(site);
  if (it == refresh_times_.end()) {
    return false;
  }

  it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
                                  [](base::TimeTicks time) {
                                    return base::TimeTicks::Now() - time >=
                                           kRefreshQuotaInterval;
                                  }),
                   it->second.end());

  size_t refresh_count = it->second.size();
  if (refresh_count == 0) {
    refresh_times_.erase(it);
  }

  return refresh_count >= kRefreshQuota;
}

void SessionServiceImpl::RemoveFetcher(RegistrationFetcher* fetcher) {
  if (!fetcher) {
    return;
  }
  auto it = registration_fetchers_.find(fetcher);
  if (it == registration_fetchers_.end()) {
    return;
  }
  registration_fetchers_.erase(it);
}

}  // namespace net::device_bound_sessions
