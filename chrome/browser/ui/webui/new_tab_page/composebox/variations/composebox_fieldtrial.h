// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

class Profile;

namespace ntp_composebox {

inline constexpr char kConfigParamParseSuccessHistogram[] =
    "NewTabPage.Composebox.ConfigParseSuccess";

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature.
BASE_DECLARE_FEATURE(kNtpComposebox);

// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to send the lns_surface parameter.
// TODO(crbug.com/430070871): Remove this flag once the server supports the
// `lns_surface` parameter.
extern const base::FeatureParam<bool> kSendLnsSurfaceParam;
// If kSendLnsSurfaceParam is true, whether to suppress the `lns_surface`
// parameter if there is no image upload. Does nothing if kSendLnsSurfaceParam
// is false.
extern const base::FeatureParam<bool> kSuppressLnsSurfaceParamIfNoImage;
// Whether or not to use separate request ids for viewport images if the
// multi-context input flow is enabled.
extern const base::FeatureParam<bool>
    kUseSeparateRequestIdsForMultiContextViewportImages;

// Whether to show zps suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxZps;
// Whether to show typed suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxTypedSuggest;
// Whether to show image suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxImageSuggestions;
// Whether to show the + entrypoint and contextual input menu in the realbox and
// composebox.
extern const base::FeatureParam<bool> kShowContextMenu;
// Whether to show the recent tab chip in the realbox and composebox.
extern const base::FeatureParam<bool> kShowRecentTabChip;
// Whether to show tab previews on hover for the composebox context menu.
extern const base::FeatureParam<bool> kShowContextMenuTabPreviews;
// The maximum number of tab suggestions to show in the composebox context menu.
extern const base::FeatureParam<int> kContextMenuMaxTabSuggestions;

// The maximum number of file attachments to upload.
extern const base::FeatureParam<int> kMaxNumFiles;
// Whether or not to show a description in the context menu entrypoint, or just
// the icon.
extern const base::FeatureParam<bool> kShowContextMenuDescription;
// Whether or not to enable viewport images with page context uploads.
extern const base::FeatureParam<bool> kEnableViewportImages;
// Whether to show the tools and models picker in the composebox.
extern const base::FeatureParam<bool> kShowToolsAndModels;
// Whether to show the create image button in the composebox context menu.
extern const base::FeatureParam<bool> kShowCreateImageTool;
// Whether to force tools and models to show in the composebox context menu.
extern const base::FeatureParam<bool> kForceToolsAndModels;

// Whether to show the submit button in the composebox.
extern const base::FeatureParam<bool> kShowSubmit;

bool IsNtpComposeboxEnabled(Profile* profile);

bool IsDeepSearchEnabled(Profile* profile);

bool IsCreateImagesEnabled(Profile* profile);

// Helper to create a QueryControllerConfigParams object from the feature
// params.
std::unique_ptr<ComposeboxQueryController::QueryControllerConfigParams>
CreateQueryControllerConfigParams();

class FeatureConfig : public omnibox_feature_configs::Config<FeatureConfig> {
 public:
  // The configuration proto for the feature.
  omnibox::NTPComposeboxConfig config;

  FeatureConfig(const FeatureConfig&);
  FeatureConfig(FeatureConfig&&);
  FeatureConfig& operator=(const FeatureConfig&);
  FeatureConfig& operator=(FeatureConfig&&);
  ~FeatureConfig();

 private:
  friend class omnibox_feature_configs::Config<FeatureConfig>;
  friend class omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;
  FeatureConfig();
};

using ScopedFeatureConfigForTesting =
    omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;

}  // namespace ntp_composebox

namespace ntp_realbox {

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature.
BASE_DECLARE_FEATURE(kNtpRealboxNext);

// Whether to show a series of cycling placeholder texts on the search input UI.
extern const base::FeatureParam<bool> kCyclingPlaceholders;

// Enum for `kRealboxLayoutMode`.
enum class RealboxLayoutMode {
  kTallBottomContext,
  kTallTopContext,
  kCompact,
};

// Flag to control the realbox layout mode (Tall, Compact).
extern const base::FeatureParam<RealboxLayoutMode> kRealboxLayoutMode;

bool IsNtpRealboxNextEnabled(Profile* profile);

// String constants for RealboxLayoutMode.
inline constexpr char kRealboxLayoutModeTallBottomContext[] =
    "TallBottomContext";
inline constexpr char kRealboxLayoutModeTallTopContext[] = "TallTopContext";
inline constexpr char kRealboxLayoutModeCompact[] = "Compact";

// Returns the string representation of `RealboxLayoutMode`.
std::string_view RealboxLayoutModeToString(
    RealboxLayoutMode realbox_layout_mode);

}  // namespace ntp_realbox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
