// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_get_credentials_helper.h"

#include <ranges>

#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_form_finder.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/origin.h"

namespace actor_login {

namespace {

using autofill::SavePasswordProgressLogger;
using password_manager::BrowserSavePasswordProgressLogger;
using password_manager::PasswordManagerClient;

using Logger = autofill::SavePasswordProgressLogger;

std::unique_ptr<BrowserSavePasswordProgressLogger> GetLogger(
    PasswordManagerClient* client) {
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<BrowserSavePasswordProgressLogger>(log_manager);
  }

  return nullptr;
}

void LogStatus(BrowserSavePasswordProgressLogger* logger,
               SavePasswordProgressLogger::StringID label,
               const std::string& value = "") {
  if (!logger) {
    return;
  }
  if (value.empty()) {
    logger->LogMessage(label);
  } else {
    logger->LogString(label, value);
  }
}

void LogStatus(BrowserSavePasswordProgressLogger* logger,
               SavePasswordProgressLogger::StringID label,
               bool value) {
  if (!logger) {
    return;
  }
  logger->LogBoolean(label, value);
}

void LogStatus(BrowserSavePasswordProgressLogger* logger,
               SavePasswordProgressLogger::StringID label,
               int value) {
  if (!logger) {
    return;
  }
  logger->LogNumber(label, value);
}

Credential PasswordFormToCredential(
    url::Origin request_origin,
    bool immediately_available_to_login,
    const password_manager::PasswordForm& form) {
  CHECK(form.match_type);
  CHECK_NE(form.match_type.value(),
           password_manager::PasswordForm::MatchType::kGrouped);
  Credential credential;
  credential.username = form.username_value;
  credential.source_site_or_app =
      ActorLoginFormFinder::GetSourceSiteOrAppFromUrl(form.url);
  credential.request_origin = request_origin;
  credential.immediatelyAvailableToLogin = immediately_available_to_login;
  credential.has_persistent_permission = form.actor_login_approved;
  return credential;
}

// Goes through all matches and either picks the first non-weak match with
// permission or returns all matches as `Credential`.
std::vector<Credential> ConstructCredentialsList(
    base::span<const password_manager::PasswordForm> best_matches,
    const url::Origin& request_origin,
    bool immediately_available_to_login) {
  std::vector<Credential> result;
  for (const auto& form : best_matches) {
    if (form.actor_login_approved &&
        !password_manager_util::IsCredentialWeakMatch(form)) {
      return {PasswordFormToCredential(request_origin,
                                       immediately_available_to_login, form)};
    }
    result.push_back(PasswordFormToCredential(
        request_origin, immediately_available_to_login, form));
  }

  return result;
}

}  // namespace

ActorLoginGetCredentialsHelper::ActorLoginGetCredentialsHelper(
    const url::Origin& origin,
    password_manager::PasswordManagerClient* client,
    password_manager::PasswordManagerInterface* password_manager,
    CredentialsOrErrorReply callback)
    : request_origin_(origin),
      callback_(std::move(callback)),
      password_manager_(password_manager) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger = GetLogger(client);
  LogStatus(logger.get(),
            Logger::STRING_ACTOR_LOGIN_GET_CREDENTIALS_FETCHING_STARTED);

  password_manager::PasswordFormCache* form_cache =
      password_manager_->GetPasswordFormCache();
  ActorLoginFormFinder login_form_finder(client);
  password_manager::PasswordFormManager* signin_form_manager =
      form_cache ? login_form_finder.GetSigninFormManager(request_origin_)
                 : nullptr;

  if (signin_form_manager) {
    immediately_available_to_login_ = true;
    form_fetcher_ = signin_form_manager->GetFormFetcher();
  } else {
    immediately_available_to_login_ = false;
    password_manager::PasswordFormDigest form_digest(
        password_manager::PasswordForm::Scheme::kHtml,
        password_manager_util::GetSignonRealm(origin.GetURL()),
        origin.GetURL());
    owned_form_fetcher_ = std::make_unique<password_manager::FormFetcherImpl>(
        std::move(form_digest), client,
        /*should_migrate_http_passwords=*/false);
    form_fetcher_ = owned_form_fetcher_.get();
    form_fetcher_->Fetch();
  }

  LogStatus(logger.get(),
            Logger::STRING_ACTOR_LOGIN_GET_CREDENTIALS_SIGNIN_FORM_EXISTS,
            immediately_available_to_login_);

  // If `form_fetcher_` has already fetched credentials before,
  // this will trigger `OnFetchCompleted` immediately (this results in
  // `ActorLoginGetCredentialsHelper` being destroyed).
  form_fetcher_->AddConsumer(this);
}

ActorLoginGetCredentialsHelper::~ActorLoginGetCredentialsHelper() = default;

void ActorLoginGetCredentialsHelper::OnFetchCompleted() {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger =
      GetLogger(password_manager_->GetClient());

  std::vector<Credential> result =
      ConstructCredentialsList(form_fetcher_->GetBestMatches(), request_origin_,
                               immediately_available_to_login_);

  CHECK(form_fetcher_);
  // Removing consumer here, as here we are sure `form_fetcher_` still exists.
  form_fetcher_->RemoveConsumer(this);

  LogStatus(logger.get(),
            Logger::STRING_ACTOR_LOGIN_GET_CREDENTIALS_NUM_CREDENTIALS,
            static_cast<int>(result.size()));

  std::move(callback_).Run(std::move(result));
}

}  // namespace actor_login
