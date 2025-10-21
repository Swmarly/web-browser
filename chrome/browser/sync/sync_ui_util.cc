// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <utility>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/singleton_tabs.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)

void OpenTabForSyncTrustedVaultUserAction(Browser* browser, const GURL& url) {
  DCHECK(browser);

  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  // Allow the window to close itself.
  params.opened_by_another_window = true;
  Navigate(&params);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
SyncStatusLabels GetSyncStatusLabelsForSettings(
    const syncer::SyncService* service) {
  // Check to see if sync has been disabled via the dashboard and needs to be
  // set up once again.
  if (!service) {
    // This can happen if Sync is disabled via the command line.
    return {SyncStatusMessageType::kPreSynced, IDS_SYNC_EMPTY_STRING,
            IDS_SYNC_EMPTY_STRING, IDS_SYNC_EMPTY_STRING,
            SyncStatusActionType::kNoAction};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (service->GetUserSettings()->IsSyncFeatureDisabledViaDashboard()) {
    return {SyncStatusMessageType::kSyncError,
            IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD,
            IDS_SYNC_EMPTY_STRING, IDS_SYNC_EMPTY_STRING,
            SyncStatusActionType::kNoAction};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // If first setup is in progress, show an "in progress" message.
  if (service->IsSetupInProgress()) {
    return {SyncStatusMessageType::kPreSynced, IDS_SYNC_SETUP_IN_PROGRESS,
            IDS_SYNC_EMPTY_STRING, IDS_SYNC_EMPTY_STRING,
            SyncStatusActionType::kNoAction};
  }

  // At this point, there is no Sync error.
  if (service->IsSyncFeatureActive()) {
    return {SyncStatusMessageType::kSynced,
            service->GetUserSettings()->IsSyncEverythingEnabled()
                ? IDS_SYNC_ACCOUNT_SYNCING
                : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES,
            IDS_SYNC_EMPTY_STRING, IDS_SYNC_EMPTY_STRING,
            SyncStatusActionType::kNoAction};
  }

  // Sync is still initializing.
  return {SyncStatusMessageType::kSynced, IDS_SYNC_EMPTY_STRING,
          IDS_SYNC_EMPTY_STRING, IDS_SYNC_EMPTY_STRING,
          SyncStatusActionType::kNoAction};
}

SyncStatusLabels GetAvatarSyncErrorLabelsForSettings(
    Profile* profile,
    syncer::SyncService::UserActionableError error) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return {SyncStatusMessageType::kSyncError, IDS_SYNC_RELOGIN_ERROR,
              IDS_SYNC_RELOGIN_BUTTON, IDS_SYNC_EMPTY_STRING,
              SyncStatusActionType::kReauthenticate};

    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return {SyncStatusMessageType::kPasswordsOnlySyncError,
              IDS_SETTINGS_ERROR_PASSWORDS_USER_ERROR_DESCRIPTION,
              IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
              IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
              SyncStatusActionType::kRetrieveTrustedVaultKeys};

    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      return {
          SyncStatusMessageType::kPasswordsOnlySyncError,
          IDS_SETTINGS_ERROR_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_USER_ERROR_DESCRIPTION,
          IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON, IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
          SyncStatusActionType::kRetrieveTrustedVaultKeys};

    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return {SyncStatusMessageType::kSyncError,
              IDS_SETTINGS_ERROR_PASSPHRASE_USER_ERROR_DESCRIPTION_WITH_EMAIL,
              IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON,
              base::FeatureList::IsEnabled(
                  syncer::kReplaceSyncPromosWithSignInPromos)
                  ? IDS_SETTINGS_PEOPLE_SIGN_OUT
                  : IDS_SETTINGS_SIGN_OUT,
              SyncStatusActionType::kEnterPassphrase};

    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return {SyncStatusMessageType::kSyncError,
              IDS_SETTINGS_ERROR_TRUSTED_VAULT_USER_ERROR_DESCRIPTION,
              IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
              IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
              SyncStatusActionType::kRetrieveTrustedVaultKeys};

    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      return {SyncStatusMessageType::kSyncError,
              IDS_SETTINGS_ERROR_UPGRADE_CLIENT_USER_ERROR_DESCRIPTION,
              IDS_SYNC_UPGRADE_CLIENT_BUTTON, IDS_SETTINGS_SIGN_OUT,
              SyncStatusActionType::kUpgradeClient};

    case syncer::SyncService::UserActionableError::kNeedsSettingsConfirmation:
      return {SyncStatusMessageType::kSyncError,
              IDS_SYNC_SETTINGS_NOT_CONFIRMED,
              IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
              IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
              SyncStatusActionType::kConfirmSyncSettings};

    case syncer::SyncService::UserActionableError::kUnrecoverableError:
      // Managed users get different labels.
      if (!ChromeSigninClientFactory::GetForProfile(profile)
               ->IsClearPrimaryAccountAllowed()) {
        return {SyncStatusMessageType::kSyncError,
                IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT,
                IDS_SYNC_RELOGIN_BUTTON, IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
                SyncStatusActionType::kReauthenticate};
      }
      return {SyncStatusMessageType::kSyncError,
              IDS_SYNC_STATUS_UNRECOVERABLE_ERROR, IDS_SYNC_RELOGIN_BUTTON,
              IDS_PROFILES_ACCOUNT_REMOVAL_TITLE,
              SyncStatusActionType::kReauthenticate};
  }
}

std::u16string GetAvatarSyncErrorDescription(
    syncer::SyncService::UserActionableError error,
    const std::string& user_email) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PAUSED_TITLE);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_PASSWORDS_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_TRUSTED_VAULT_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_PASSPHRASE_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_UPGRADE_CLIENT_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return l10n_util::GetStringFUTF16(
          IDS_SYNC_ERROR_TRUSTED_VAULT_USER_MENU_ERROR_DESCRIPTION,
          base::UTF8ToUTF16(user_email));
    case syncer::SyncService::UserActionableError::kNeedsSettingsConfirmation:
    case syncer::SyncService::UserActionableError::kUnrecoverableError:
      return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ShouldRequestSyncConfirmation(const syncer::SyncService* service) {
  // This method mainly handles the situation where the initial Sync setup was
  // aborted without actually disabling Sync again. That generally shouldn't
  // happen, but it might if Chrome crashed while the setup was ongoing, or due
  // to past bugs in the setup flow.
  return !service->IsLocalSyncEnabled() && service->HasSyncConsent() &&
         !service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
}

bool ShouldShowSyncPassphraseError(const syncer::SyncService* service) {
  const syncer::SyncUserSettings* settings = service->GetUserSettings();
  if (service->HasSyncConsent() &&
      !settings->IsInitialSyncFeatureSetupComplete()) {
    return false;
  }
  return settings->IsPassphraseRequiredForPreferredDataTypes();
}

#if !BUILDFLAG(IS_ANDROID)
void OpenTabForSyncKeyRetrieval(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger) {
  RecordKeyRetrievalTrigger(trigger);
  const GURL continue_url =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue());
  GURL retrieval_url =
      GaiaUrls::GetInstance()->signin_chrome_sync_keys_retrieval_url();
  if (continue_url.is_valid()) {
    retrieval_url = net::AppendQueryParameter(retrieval_url, "continue",
                                              continue_url.spec());
  }
  OpenTabForSyncTrustedVaultUserAction(browser, retrieval_url);
}

void OpenTabForSyncKeyRecoverabilityDegraded(
    Browser* browser,
    syncer::TrustedVaultUserActionTriggerForUMA trigger) {
  RecordRecoverabilityDegradedFixTrigger(trigger);
  const GURL continue_url =
      GURL(UIThreadSearchTermsData().GoogleBaseURLValue());
  GURL url = GaiaUrls::GetInstance()
                 ->signin_chrome_sync_keys_recoverability_degraded_url();
  if (continue_url.is_valid()) {
    url = net::AppendQueryParameter(url, "continue", continue_url.spec());
  }
  OpenTabForSyncTrustedVaultUserAction(browser, url);
}
#endif  // !BUILDFLAG(IS_ANDROID)
