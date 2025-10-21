// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"

#include <memory>
#include <utility>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/lens/lens_features.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/composebox/contextual_session_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

class RealboxOmniboxClient final : public ContextualOmniboxClient {
 public:
  RealboxOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~RealboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  void OnBookmarkLaunched() override;
};

RealboxOmniboxClient::RealboxOmniboxClient(Profile* profile,
                                           content::WebContents* web_contents)
    : ContextualOmniboxClient(profile, web_contents) {}

RealboxOmniboxClient::~RealboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
RealboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::NTP_REALBOX;
}

void RealboxOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BookmarkLaunchLocation::kOmnibox,
                       profile_metrics::GetBrowserProfileType(profile_));
}

}  // namespace

RealboxHandler::RealboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    std::unique_ptr<ComposeboxMetricsRecorder> composebox_metrics_recorder,
    Profile* profile,
    content::WebContents* web_contents)
    : ContextualSearchboxHandler(
          std::move(pending_page_handler),
          profile,
          web_contents,
          std::move(composebox_metrics_recorder),
          std::make_unique<OmniboxController>(
              /*view=*/nullptr,
              std::make_unique<RealboxOmniboxClient>(profile, web_contents),
              kAutocompleteDefaultStopTimerDuration)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

RealboxHandler::~RealboxHandler() = default;
