// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

namespace {

// UMA histograms:
// Histogram for the eligibility request status.
static constexpr char kEligibilityRequestStatusHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestStatus";
// Histogram for the eligibility request response code.
static constexpr char kEligibilityRequestResponseCodeHistogramName[] =
    "Omnibox.AimEligibility.EligibilityResponseCode";
// Histogram for the eligibility response source.
static constexpr char kEligibilityResponseSourceHistogramName[] =
    "Omnibox.AimEligibility.EligibilityResponseSource";
// Histogram prefix for the eligibility response.
static constexpr char kEligibilityResponseHistogramPrefix[] =
    "Omnibox.AimEligibility.EligibilityResponse";
// Histogram prefix for changes to the eligibility response.
static constexpr char kEligibilityResponseChangeHistogramPrefix[] =
    "Omnibox.AimEligibility.EligibilityResponseChange";

static constexpr char kRequestPath[] = "/async/folae";
static constexpr char kRequestQuery[] = "async=_fmt:pb";

// Reflects the default value for the `kAIModeSettings` pref; 0 = allowed, 1 =
// disallowed. Pref value is determined by: `AIModeSettings` policy,
// `GenAiDefaultSettings` policy if `AIModeSettings` isn't set, or the default
// pref value (0) if neither policy is set. Do not change this value without
// migrating the existing prefs and the policy's prefs mapping.
constexpr int kAiModeAllowedDefault = 0;

// The pref name used for storing the eligibility response proto.
constexpr char kResponsePrefName[] =
    "aim_eligibility_service.aim_eligibility_response";

// Returns the request URL or an empty GURL if a valid URL cannot be created;
// e.g., Google is not the default search provider.
GURL GetRequestUrl(const TemplateURLService* template_url_service) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service)) {
    return GURL();
  }

  GURL base_gurl(
      template_url_service->search_terms_data().GoogleBaseURLValue());
  if (!base_gurl.is_valid()) {
    return GURL();
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(kRequestPath);
  replacements.SetQueryStr(kRequestQuery);
  return base_gurl.ReplaceComponents(replacements);
}

const net::NetworkTrafficAnnotationTag kRequestTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("aim_eligibility_fetch", R"(
      semantics {
        sender: "Chrome AI Mode Eligibility Service"
        description:
          "Retrieves the set of AI Mode features the client is eligible for "
          "from the server."
        trigger:
          "Requests are made on startup, when user's profile state changes, "
          "and periodically while Chrome is running."
        user_data {
          type: NONE
        }
        data:
          "No request body is sent; this is a GET request with no query params."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts { email: "chrome-desktop-search@google.com" }
        }
        last_reviewed: "2025-08-06"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "Coupled to Google default search."
        policy_exception_justification:
          "Not gated by policy. Setting AIModeSetting to '1' prevents the "
          "response from being used. But Google Chrome still makes the "
          "requests and saves the response to disk so that it's available when "
          "the policy is unset."
      })");

// Parses `response_string` into `response_proto`. Does not modify
// `response_proto` if parsing fails. Returns false on failure.
bool ParseResponseString(const std::string& response_string,
                         omnibox::AimEligibilityResponse* response_proto) {
  omnibox::AimEligibilityResponse proto;
  if (!proto.ParseFromString(response_string)) {
    return false;
  }
  *response_proto = proto;
  return true;
}

// Reads `kResponsePrefName` and parses it into `response_proto`. Does not
// modify `response_proto` if parsing fails. Returns false on failure.
bool GetResponseFromPrefs(const PrefService* prefs,
                          omnibox::AimEligibilityResponse* response_proto) {
  std::string encoded_response = prefs->GetString(kResponsePrefName);
  if (encoded_response.empty()) {
    return false;
  }
  std::string response_string;
  if (!base::Base64Decode(encoded_response, &response_string)) {
    return false;
  }
  if (!ParseResponseString(response_string, response_proto)) {
    return false;
  }
  return true;
}

}  // namespace

// static
bool AimEligibilityService::GenericKillSwitchFeatureCheck(
    const AimEligibilityService* aim_eligibility_service,
    const base::Feature& feature,
    const std::optional<std::reference_wrapper<const base::Feature>>
        feature_en_us) {
  if (!aim_eligibility_service) {
    return false;
  }

  // If not locally eligible, return false.
  if (!aim_eligibility_service->IsAimLocallyEligible()) {
    return false;
  }

  // If the generic feature is overridden, it takes precedence.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverridden(feature.name)) {
    return base::FeatureList::IsEnabled(feature);
  }

  // If the server eligibility is enabled, check overall eligibility alone.
  // The service will control locale rollout so there's no need to check locale
  // or the state of kMyFeature below.
  if (aim_eligibility_service->IsServerEligibilityEnabled()) {
    return aim_eligibility_service->IsAimEligible();
  }

  // Otherwise, check the generic entrypoint feature default value.
  return base::FeatureList::IsEnabled(feature) ||
         (feature_en_us &&
          base::FeatureList::IsEnabled(feature_en_us.value()) &&
          aim_eligibility_service->IsLanguage("en") &&
          aim_eligibility_service->IsCountry("us"));
}

// static
void AimEligibilityService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kResponsePrefName, "");
  registry->RegisterIntegerPref(omnibox::kAIModeSettings,
                                kAiModeAllowedDefault);
}

// static
bool AimEligibilityService::IsAimAllowedByPolicy(const PrefService* prefs) {
  return prefs->GetInteger(omnibox::kAIModeSettings) == kAiModeAllowedDefault;
}

AimEligibilityService::AimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  if (base::FeatureList::IsEnabled(omnibox::kAimEnabled)) {
    Initialize();
  }
}

AimEligibilityService::~AimEligibilityService() = default;

bool AimEligibilityService::IsCountry(const std::string& country) const {
  // Country codes are in lowercase ISO 3166-1 alpha-2 format; e.g., us, br, in.
  // See components/variations/service/variations_service.h
  return GetCountryCode() == country;
}

bool AimEligibilityService::IsLanguage(const std::string& language) const {
  // Locale follows BCP 47 format; e.g., en-US, fr-FR, ja-JP.
  // See ui/base/l10n/l10n_util.h
  return base::StartsWith(GetLocale(), language, base::CompareCase::SENSITIVE);
}

base::CallbackListSubscription
AimEligibilityService::RegisterEligibilityChangedCallback(
    base::RepeatingClosure callback) {
  return eligibility_changed_callbacks_.Add(callback);
}

bool AimEligibilityService::IsServerEligibilityEnabled() const {
  return base::FeatureList::IsEnabled(omnibox::kAimServerEligibilityEnabled);
}

bool AimEligibilityService::IsAimLocallyEligible() const {
  // Kill switch: If AIM is completely disabled, return false.
  if (!base::FeatureList::IsEnabled(omnibox::kAimEnabled)) {
    return false;
  }

  // Always check Google DSE and Policy requirements.
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !IsAimAllowedByPolicy(&pref_service_.get())) {
    return false;
  }

  return true;
}

bool AimEligibilityService::IsAimEligible() const {
  // Check local eligibility first.
  if (!IsAimLocallyEligible()) {
    return false;
  }

  // Conditionally check server response eligibility requirement.
  if (IsServerEligibilityEnabled()) {
    base::UmaHistogramEnumeration(kEligibilityResponseSourceHistogramName,
                                  most_recent_response_source_);
    return most_recent_response_.is_eligible();
  }

  return true;
}

bool AimEligibilityService::IsPdfUploadEligible() const {
  if (!IsAimEligible()) {
    return false;
  }

  if (IsServerEligibilityEnabled()) {
    return most_recent_response_.is_pdf_upload_eligible();
  }

  return true;
}

bool AimEligibilityService::IsDeepSearchEligible() const {
  if (!IsAimEligible()) {
    return false;
  }

  if (IsServerEligibilityEnabled()) {
    return most_recent_response_.is_deep_search_eligible();
  }

  return true;
}

bool AimEligibilityService::IsCreateImagesEligible() const {
  if (!IsAimEligible()) {
    return false;
  }

  if (IsServerEligibilityEnabled()) {
    return most_recent_response_.is_image_generation_eligible();
  }

  return true;
}

// Private methods -------------------------------------------------------------

void AimEligibilityService::Initialize() {
  // The service should not be initialized if AIM is disabled.
  CHECK(base::FeatureList::IsEnabled(omnibox::kAimEnabled));
  // The service should not be initialized twice.
  CHECK(!initialized_);

  if (!template_url_service_) {
    return;
  }

  if (!template_url_service_->loaded()) {
    template_url_service_subscription_ =
        template_url_service_->RegisterOnLoadedCallback(base::BindOnce(
            &AimEligibilityService::Initialize, weak_factory_.GetWeakPtr()));
    return;
  }

  initialized_ = true;

  pref_change_registrar_.Init(&pref_service_.get());
  pref_change_registrar_.Add(
      kResponsePrefName,
      base::BindRepeating(&AimEligibilityService::OnEligibilityResponseChanged,
                          weak_factory_.GetWeakPtr()));

  LoadMostRecentResponse();

  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnStartupEnabled)) {
    StartServerEligibilityRequest(RequestSource::kStartup);
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

void AimEligibilityService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnPrimaryAccountChanges.Get()) {
    return;
  }
  // Change to the primary account might affect AIM eligibility.
  // Refresh the server eligibility state.
  StartServerEligibilityRequest(RequestSource::kPrimaryAccountChange);
}

void AimEligibilityService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnCookieJarChanges.Get()) {
    return;
  }
  // Change to the accounts in the cookie jar might affect AIM eligibility.
  // Refresh the server eligibility state.
  StartServerEligibilityRequest(RequestSource::kCookieChange);
}

void AimEligibilityService::OnEligibilityResponseChanged() {
  CHECK(initialized_);

  LogEligibilityResponseChange();

  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilityChangedNotification)) {
    eligibility_changed_callbacks_.Notify();
  }
}

void AimEligibilityService::UpdateMostRecentResponse(
    const omnibox::AimEligibilityResponse& response_proto) {
  CHECK(initialized_);

  std::string response_string;
  response_proto.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);
  pref_service_->SetString(kResponsePrefName, encoded_response);

  most_recent_response_ = response_proto;
  most_recent_response_source_ = EligibilityResponseSource::kServer;
}

void AimEligibilityService::LoadMostRecentResponse() {
  CHECK(initialized_);

  omnibox::AimEligibilityResponse prefs_response;
  if (!GetResponseFromPrefs(&pref_service_.get(), &prefs_response)) {
    return;
  }

  most_recent_response_ = prefs_response;
  most_recent_response_source_ = EligibilityResponseSource::kPrefs;
}

void AimEligibilityService::StartServerEligibilityRequest(
    RequestSource request_source) {
  CHECK(initialized_);

  // URLLoaderFactory may be null in tests.
  if (!url_loader_factory_) {
    return;
  }

  // Request URL may be invalid.
  GURL request_url = GetRequestUrl(template_url_service_.get());
  if (!request_url.is_valid()) {
    return;
  }

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = request_url;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  // Set the SiteForCookies to the request URL's site to avoid cookie blocking.
  request->site_for_cookies = net::SiteForCookies::FromUrl(request->url);
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       kRequestTrafficAnnotation);

  LogEligibilityRequestStatus(EligibilityRequestStatus::kSent, request_source);

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AimEligibilityService::OnServerEligibilityResponse,
                     weak_factory_.GetWeakPtr(), std::move(loader),
                     request_source));
}

void AimEligibilityService::OnServerEligibilityResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    RequestSource request_source,
    std::unique_ptr<std::string> response_string) {
  CHECK(initialized_);

  const int response_code =
      loader->ResponseInfo() && loader->ResponseInfo()->headers
          ? loader->ResponseInfo()->headers->response_code()
          : 0;

  LogEligibilityRequestResponseCode(response_code, request_source);

  if (response_code != 200 || !response_string) {
    LogEligibilityRequestStatus(EligibilityRequestStatus::kErrorResponse,
                                request_source);
    return;
  }
  omnibox::AimEligibilityResponse response_proto;
  if (!ParseResponseString(*response_string, &response_proto)) {
    LogEligibilityRequestStatus(EligibilityRequestStatus::kFailedToParse,
                                request_source);
    return;
  }
  LogEligibilityRequestStatus(EligibilityRequestStatus::kSuccess,
                              request_source);

  UpdateMostRecentResponse(response_proto);
  LogEligibilityResponse(request_source);
}

std::string AimEligibilityService::GetHistogramNameSlicedByRequestSource(
    const std::string& histogram_name,
    RequestSource request_source) const {
  auto request_source_suffix = [](RequestSource request_source) {
    switch (request_source) {
      case RequestSource::kStartup:
        return ".Startup";
      case RequestSource::kCookieChange:
        return ".CookieChange";
      case RequestSource::kPrimaryAccountChange:
        return ".PrimaryAccountChange";
    }
    return "";
  };
  return base::StrCat({histogram_name, request_source_suffix(request_source)});
}

void AimEligibilityService::LogEligibilityRequestStatus(
    EligibilityRequestStatus status,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestStatusHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramEnumeration(name, status);
  base::UmaHistogramEnumeration(sliced_name, status);
}

void AimEligibilityService::LogEligibilityRequestResponseCode(
    int response_code,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestResponseCodeHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramSparse(name, response_code);
  base::UmaHistogramSparse(sliced_name, response_code);
}

void AimEligibilityService::LogEligibilityResponse(
    RequestSource request_source) const {
  const auto& prefix = kEligibilityResponseHistogramPrefix;
  const auto& sliced_prefix =
      GetHistogramNameSlicedByRequestSource(prefix, request_source);
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_eligible"}),
                            most_recent_response_.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({sliced_prefix, ".is_eligible"}),
                            most_recent_response_.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_pdf_upload_eligible"}),
                            most_recent_response_.is_pdf_upload_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_pdf_upload_eligible"}),
      most_recent_response_.is_pdf_upload_eligible());
  base::UmaHistogramSparse(base::StrCat({prefix, ".session_index"}),
                           most_recent_response_.session_index());
  base::UmaHistogramSparse(base::StrCat({sliced_prefix, ".session_index"}),
                           most_recent_response_.session_index());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_deep_search_eligible"}),
                            most_recent_response_.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_deep_search_eligible"}),
      most_recent_response_.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_image_generation_eligible"}),
      most_recent_response_.is_image_generation_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_image_generation_eligible"}),
      most_recent_response_.is_image_generation_eligible());
}

void AimEligibilityService::LogEligibilityResponseChange() const {
  // Prefs are updated before `most_recent_response_` is. Compare the prefs with
  // the previous state of the server response and log changes to each field.
  omnibox::AimEligibilityResponse prefs_response;
  if (!GetResponseFromPrefs(&pref_service_.get(), &prefs_response)) {
    return;
  }

  const auto& prefix = kEligibilityResponseChangeHistogramPrefix;
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_eligible"}),
      most_recent_response_.is_eligible() != prefs_response.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_pdf_upload_eligible"}),
                            most_recent_response_.is_pdf_upload_eligible() !=
                                prefs_response.is_pdf_upload_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".session_index"}),
      most_recent_response_.session_index() != prefs_response.session_index());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_deep_search_eligible"}),
                            most_recent_response_.is_deep_search_eligible() !=
                                prefs_response.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_image_generation_eligible"}),
      most_recent_response_.is_image_generation_eligible() !=
          prefs_response.is_image_generation_eligible());
}
