// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ai_mode_page_action_icon_view.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/branded_strings.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

AiModePageActionIconView::AiModePageActionIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    BrowserWindowInterface* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "AiMode",
                         kActionAiMode),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);

  pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_registrar_->Init(browser_->GetProfile()->GetPrefs());
  pref_registrar_->Add(omnibox::kShowAiModeOmniboxButton,
                       base::BindRepeating(&AiModePageActionIconView::Update,
                                           base::Unretained(this)));

  SetProperty(views::kElementIdentifierKey, kAiModePageActionIconElementId);

  SetLabel(l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL));
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);

  // The accessible name prompts the user to ask Google AI Mode.
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS),
      ax::mojom::NameFrom::kAttribute);
}

AiModePageActionIconView::~AiModePageActionIconView() = default;

void AiModePageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  OmniboxView* omnibox_view = GetOmniboxView();
  CHECK(omnibox_view);
  omnibox::AiModePageActionController::OpenAiMode(*omnibox_view,
                                                  /*via_keyboard=*/false);
}

views::BubbleDialogDelegate* AiModePageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& AiModePageActionIconView::GetVectorIcon() const {
  return omnibox::kSearchSparkIcon;
}

// This event handler exists because, on Mac, the <return> key doesn't activate
// buttons in the omnibox or on the toolbelt. However, this page action is
// designed to act like part of the popup when the popup is open and <return>
// activates it in that state. In order to have consistent behavior, this event
// handler ensures that <return> still activates the behavior when the popup
// *isn't* open.
//
// Other platforms don't require this, so it could be guarded by an IS_MAC build
// flag. However, using this same code path on all platforms may help avoid
// platform-specific bugs.
bool AiModePageActionIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    OmniboxView* omnibox_view = GetOmniboxView();
    CHECK(omnibox_view);
    omnibox::AiModePageActionController::OpenAiMode(*omnibox_view,
                                                    /*via_keyboard=*/true);
    return true;
  }

  return PageActionIconView::OnKeyPressed(event);
}

void AiModePageActionIconView::ExecuteWithKeyboardSourceForTesting() {
  CHECK(GetVisible());
  OnExecuting(EXECUTE_SOURCE_KEYBOARD);
}

void AiModePageActionIconView::UpdateImpl() {
  Profile* profile = browser_->GetProfile();
  bool enabled =
      profile->GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton);
  views::View* location_bar_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kLocationBarElementId,
          views::ElementTrackerViews::GetContextForView(this));
  OmniboxView* omnibox_view = GetOmniboxView();
  if (!profile || !location_bar_view || !omnibox_view) {
    return;
  }

  const bool is_visible =
      enabled && omnibox::AiModePageActionController::ShouldShowPageAction(
                     profile, *location_bar_view, *omnibox_view);
  if (is_visible) {
    omnibox::AiModePageActionController::NotifyOmniboxTriggeredFeatureService(
        *omnibox_view);
  }
  SetVisible(is_visible);
  ResetSlideAnimation(true);
}

OmniboxView* AiModePageActionIconView::GetOmniboxView() {
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return search::GetOmniboxView(web_contents);
}

BEGIN_METADATA(AiModePageActionIconView)
END_METADATA
