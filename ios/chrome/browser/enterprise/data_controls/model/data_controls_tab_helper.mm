// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/web_state.h"
#import "ui/base/clipboard/clipboard_metadata.h"
#import "ui/base/l10n/l10n_util.cc"
#import "url/gurl.h"

namespace data_controls {

DataControlsTabHelper::DataControlsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

DataControlsTabHelper::~DataControlsTabHelper() = default;

void DataControlsTabHelper::ShouldAllowCopy(
    base::OnceCallback<void(bool)> callback) {
  if (!IsClipboardDataControlsEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  // TODO(crbug.com/444224082): Include size and format type for copy
  // operations.
  ui::ClipboardMetadata metadata;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& source_url = web_state_->GetLastCommittedURL();
  CopyPolicyVerdicts verdicts =
      IsCopyAllowedByPolicy(source_url, metadata, profile);

  switch (verdicts.copy_action_verdict.level()) {
    case Rule::Level::kWarn:
      ShowWarningDialog(
          DataControlsDialog::Type::kClipboardCopyWarn,
          GetManagementDomain(profile),
          base::BindOnce(&DataControlsTabHelper::FinishCopy,
                         weak_factory_.GetWeakPtr(), source_url,
                         std::move(verdicts), std::move(callback)));
      break;
    case Rule::Level::kBlock:
      ShowRestrictSnackbar(GetManagementDomain(profile));
      [[fallthrough]];
    case Rule::Level::kReport:
    case Rule::Level::kAllow:
    case Rule::Level::kNotSet:
      FinishCopy(source_url, std::move(verdicts), std::move(callback),
                 /*bypassed=*/false);
      break;
  }
}

void DataControlsTabHelper::ShouldAllowPaste(
    base::OnceCallback<void(bool)> callback) {
  if (!IsClipboardDataControlsEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  // TODO(crbug.com/444224082): Include size and format type for paste
  // operations.
  ui::ClipboardMetadata metadata;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& destination_url = web_state_->GetLastCommittedURL();

  DataControlsPasteboardManager* pasteboard_manager =
      DataControlsPasteboardManager::GetInstance();
  PasteboardSource source =
      pasteboard_manager->GetCurrentPasteboardItemsSource();

  PastePolicyVerdict policy_verdict =
      IsPasteAllowedByPolicy(source.source_url, destination_url, metadata,
                             source.source_profile, profile);
  std::string domain = policy_verdict.dialog_triggered_by_source
                           ? GetManagementDomain(source.source_profile)
                           : GetManagementDomain(profile);

  switch (policy_verdict.verdict.level()) {
    case Rule::Level::kWarn:
      ShowWarningDialog(
          DataControlsDialog::Type::kClipboardPasteWarn, domain,
          base::BindOnce(&DataControlsTabHelper::FinishPaste,
                         weak_factory_.GetWeakPtr(), destination_url,
                         std::move(policy_verdict.verdict),
                         std::move(callback)));
      break;
    case Rule::Level::kBlock:
      ShowRestrictSnackbar(domain);
      [[fallthrough]];
    case Rule::Level::kReport:
    case Rule::Level::kAllow:
    case Rule::Level::kNotSet:
      FinishPaste(destination_url, std::move(policy_verdict.verdict),
                  std::move(callback),
                  /*bypassed=*/false);
      break;
  }
}

void DataControlsTabHelper::ShouldAllowCut(
    base::OnceCallback<void(bool)> callback) {
  // "Cut" is treated the same way as "copy".
  ShouldAllowCopy(std::move(callback));
}

void DataControlsTabHelper::ShouldAllowShare(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::SetDataControlsCommandsHandler(
    id<DataControlsCommands> handler) {
  commands_handler_ = handler;
}

void DataControlsTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
}
void DataControlsTabHelper::DidFinishClipboardRead() {
  DataControlsPasteboardManager::GetInstance()
      ->RestorePlaceholderToGeneralPasteboardIfNeeded();
}

bool DataControlsTabHelper::IsClipboardDataControlsEnabled() const {
  return base::FeatureList::IsEnabled(kEnableClipboardDataControlsIOS);
}

void DataControlsTabHelper::FinishCopy(const GURL& source_url,
                                       CopyPolicyVerdicts verdicts,
                                       base::OnceCallback<void(bool)> callback,
                                       bool bypassed) {
  // The user may have navigated away from the page from which the copy
  // operation was initiated. If the URL has changed, we should block the copy
  // operation as the original content is no longer available.
  if (source_url != web_state_->GetLastCommittedURL()) {
    std::move(callback).Run(false);
    return;
  }

  Verdict verdict = std::move(verdicts.copy_action_verdict);

  bool allowed = verdict.level() != Rule::Level::kBlock;
  if (verdict.level() == Rule::Level::kWarn) {
    allowed = bypassed;
  }

  if (allowed) {
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
    auto* pasteboard_manager = DataControlsPasteboardManager::GetInstance();
    pasteboard_manager->SetNextPasteboardItemsSource(
        source_url, profile, verdicts.copy_to_os_clipbord);
  }

  std::move(callback).Run(allowed);
}

void DataControlsTabHelper::FinishPaste(const GURL& destination_url,
                                        Verdict verdict,
                                        base::OnceCallback<void(bool)> callback,
                                        bool bypassed) {
  // The user may have navigated away from the page into which the paste
  // operation was initiated. If the URL has changed, we should block the paste
  // operation as the original destination is no longer available.
  if (destination_url != web_state_->GetLastCommittedURL()) {
    std::move(callback).Run(false);
    return;
  }

  bool allowed = verdict.level() != Rule::Level::kBlock;
  if (verdict.level() == Rule::Level::kWarn) {
    allowed = bypassed;
  }

  if (allowed) {
    DataControlsPasteboardManager::GetInstance()
        ->RestoreItemsToGeneralPasteboardIfNeeded(
            base::BindOnce(std::move(callback), allowed));
  } else {
    std::move(callback).Run(false);
  }
}

void DataControlsTabHelper::ShowWarningDialog(
    DataControlsDialog::Type dialog_type,
    std::string_view org_domain,
    base::OnceCallback<void(bool)> on_bypassed_callback) {
  if (commands_handler_) {
    [commands_handler_
        showDataControlsWarningDialog:dialog_type
                   organizationDomain:org_domain
                             callback:std::move(on_bypassed_callback)];
  } else {
    if (on_bypassed_callback) {
      std::move(on_bypassed_callback).Run(false);
    }
  }
}

void DataControlsTabHelper::ShowRestrictSnackbar(std::string_view org_domain) {
  NSString* message =
      org_domain.empty()
          ? l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
          : l10n_util::GetNSStringF(IDS_DATA_CONTROLS_BLOCKED_LABEL_WITH_DOMAIN,
                                    base::UTF8ToUTF16(org_domain));
  [snackbar_handler_ showSnackbarWithMessage:message
                                  buttonText:nil
                               messageAction:nil
                            completionAction:nil];
}

std::string DataControlsTabHelper::GetManagementDomain(ProfileIOS* profile) {
  if (!profile) {
    return std::string();
  }

  policy::PolicyScope scope = static_cast<policy::PolicyScope>(
      profile->GetPrefs()->GetInteger(kDataControlsRulesScopePref));
  return enterprise::GetManagementDomain(scope, profile);
}

}  // namespace data_controls
