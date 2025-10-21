// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_features.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuCoordinator::ExtensionsMenuCoordinator(Browser* browser)
    : browser_(browser) {}

ExtensionsMenuCoordinator::~ExtensionsMenuCoordinator() {
  if (views::Widget* const menu = GetExtensionsMenuWidget()) {
    // Close the menu widget synchronously as it may hold references back to the
    // coordinator and its host Browser.
    menu->CloseNow();
  }
}

void ExtensionsMenuCoordinator::Show(
    views::BubbleAnchor anchor,
    ExtensionsContainer* extensions_container) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate =
      CreateExtensionsMenuBubbleDialogDelegate(anchor, extensions_container);

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

void ExtensionsMenuCoordinator::Hide() {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  if (views::Widget* const menu = GetExtensionsMenuWidget()) {
    menu->Close();
    // Immediately stop tracking the view. Widget will be destroyed
    // asynchronously.
    bubble_tracker_.SetView(nullptr);
  }
}

bool ExtensionsMenuCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

views::Widget* ExtensionsMenuCoordinator::GetExtensionsMenuWidget() {
  return IsShowing() ? bubble_tracker_.view()->GetWidget() : nullptr;
}

std::unique_ptr<views::BubbleDialogDelegate>
ExtensionsMenuCoordinator::CreateExtensionsMenuBubbleDialogDelegateForTesting(
    views::BubbleAnchor anchor,
    ExtensionsContainer* extensions_container) {
  return CreateExtensionsMenuBubbleDialogDelegate(anchor, extensions_container);
}

std::unique_ptr<views::BubbleDialogDelegate>
ExtensionsMenuCoordinator::CreateExtensionsMenuBubbleDialogDelegate(
    views::BubbleAnchor anchor,
    ExtensionsContainer* extensions_container) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor, views::BubbleBorder::TOP_RIGHT,
      views::BubbleBorder::DIALOG_SHADOW, /*autosize=*/true);
  bubble_delegate->SetOwnedByWidget(
      views::WidgetDelegate::OwnedByWidgetPassKey());
  bubble_delegate->set_margins(gfx::Insets(0));
  bubble_delegate->set_fixed_width(
      views::LayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_EXTENSIONS_MENU_WIDTH));
  // Let anchor view's MenuButtonController handle the highlight.
  bubble_delegate->set_highlight_button_when_shown(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->SetEnableArrowKeyTraversal(true);

  auto* bubble_contents = bubble_delegate->SetContentsView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());
  bubble_view_observation_.Observe(bubble_contents);
  bubble_tracker_.SetView(bubble_contents);

  auto menu_delegate =
      std::make_unique<ExtensionsMenuViewPlatformDelegateViews>(
          browser_, extensions_container, bubble_contents);
  menu_delegate_ = menu_delegate.get();
  menu_model_ = std::make_unique<ExtensionsMenuViewModel>(
      browser_, std::move(menu_delegate));

  menu_delegate_->OpenMainPage();

  return bubble_delegate;
}

void ExtensionsMenuCoordinator::OnViewIsDeleting(views::View* observed_view) {
  bubble_tracker_.SetView(nullptr);
  bubble_view_observation_.Reset();
  // Reset the model to keep 1:1 lifetime with the view. The model owns the
  // delegate, so it must be set to nullptr first.
  menu_delegate_ = nullptr;
  menu_model_.reset();
}
