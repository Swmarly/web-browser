// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_handler.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_composebox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

namespace {

class LensComposeboxOmniboxClient final : public ContextualOmniboxClient {
 public:
  LensComposeboxOmniboxClient(
      Profile* profile,
      content::WebContents* web_contents,
      lens::LensComposeboxController* lens_composebox_controller);

  ~LensComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

 private:
  raw_ptr<lens::LensComposeboxController> lens_composebox_controller_;
};

LensComposeboxOmniboxClient::LensComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    lens::LensComposeboxController* lens_composebox_controller)
    : ContextualOmniboxClient(profile, web_contents),
      lens_composebox_controller_(lens_composebox_controller) {}

LensComposeboxOmniboxClient::~LensComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
LensComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  // TODO(crbug.com/441808425): This page classification should be passed in
  // from the embedder so that it can be customized. Currently, Lens is logging
  // as NTP_COMPOSEBOX, but it should be its own page classification.
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
}

void LensComposeboxOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  std::string query_text;
  net::GetValueForKeyInQuery(destination_url, "q", &query_text);
  lens_composebox_controller_->IssueComposeboxQuery(query_text);
}

std::optional<lens::proto::LensOverlaySuggestInputs>
LensComposeboxOmniboxClient::GetLensOverlaySuggestInputs() const {
  return lens_composebox_controller_->GetLensSuggestInputs();
}

}  // namespace

namespace lens {

LensComposeboxHandler::LensComposeboxHandler(
    lens::LensComposeboxController* parent_controller,
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler)
    : SearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              /*view=*/nullptr,
              std::make_unique<LensComposeboxOmniboxClient>(
                  profile,
                  web_contents,
                  /*lens_composebox_controller=*/parent_controller))),
      lens_composebox_controller_(parent_controller),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

LensComposeboxHandler::~LensComposeboxHandler() = default;

void LensComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {
  lens_composebox_controller_->IssueComposeboxQuery(query_text);
}

void LensComposeboxHandler::SubmitQuery(const std::string& query_text,
                                        uint8_t mouse_button,
                                        bool alt_key,
                                        bool ctrl_key,
                                        bool meta_key,
                                        bool shift_key) {
  SubmitQuery(query_text,
              ui::DispositionFromClick(
                  /*middle_button=*/mouse_button == 1, alt_key, ctrl_key,
                  meta_key, shift_key),
              /*additional_params=*/{});
}

void LensComposeboxHandler::FocusChanged(bool focused) {
  lens_composebox_controller_->OnFocusChanged(focused);
}

void LensComposeboxHandler::SetDeepSearchMode(bool enabled) {
  // Ignore, intentionally unimplemented for Lens. Deep search not implemented
  // in Lens.
}

void LensComposeboxHandler::SetCreateImageMode(bool enabled,
                                               bool image_present) {
  // Ignore, intentionally unimplemented for Lens. Create image not implemented
  // in Lens.
}

void LensComposeboxHandler::HandleLensButtonClick() {
  lens_composebox_controller_->ShowLensSelectionOverlay();
}

void LensComposeboxHandler::DeleteAutocompleteMatch(uint8_t line,
                                                    const GURL& url) {
  NOTREACHED();
}

void LensComposeboxHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  NOTREACHED();
}

void LensComposeboxHandler::OnThumbnailRemoved() {
  NOTREACHED();
}

}  // namespace lens
