// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/handoff_button_controller.h"

#include <memory>

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace actor::ui {
namespace {

using enum actor::ui::HandoffButtonState::ControlOwnership;
using ::testing::_;
using ::ui::EventTimeForNow;
using ::ui::EventType;
using ::ui::MouseEvent;

constexpr char kActorUiHandoffButtonTakeControlClickedHistogram[] =
    "Actor.Ui.HandoffButton.TakeControl.Clicked";
constexpr char kActorUiHandoffButtonGiveControlClickedHistogram[] =
    "Actor.Ui.HandoffButton.GiveControl.Clicked";

class TestHandoffButtonController : public HandoffButtonController {
 public:
  explicit TestHandoffButtonController(tabs::TabInterface& tab_interface)
      : HandoffButtonController(tab_interface) {}
  ~TestHandoffButtonController() override = default;

  void SetWidgetAndButtonForTest(std::unique_ptr<HandoffButtonWidget> widget,
                                 views::LabelButton* button) {
    widget_ = std::move(widget);
    button_view_ = button;
  }
  void TestShouldShowButton(bool& show) { ShouldShowButton(show); }

  void TestUpdateButtonHoverStatus(bool is_hovered) {
    UpdateButtonHoverStatus(is_hovered);
  }

  // Override to verify the call without the side effect of widget deletion,
  // which interferes with the test's teardown procedure.
  void CloseButton(views::Widget::ClosedReason reason) override {
    close_button_call_count_++;
  }
  int close_button_call_count() const { return close_button_call_count_; }

  void UpdateBounds() override { update_bounds_call_count_++; }
  int update_bounds_call_count() const { return update_bounds_call_count_; }

  void UpdateVisibility() override { update_visibility_call_count_++; }
  int update_visibility_call_count() const {
    return update_visibility_call_count_;
  }

  void PressButton() { OnButtonPressed(); }

 private:
  int close_button_call_count_ = 0;
  int update_bounds_call_count_ = 0;
  int update_visibility_call_count_ = 0;
};

class HandoffButtonControllerTest : public views::ViewsTestBase {
 public:
  HandoffButtonControllerTest() {
    MockActorUiTabController::SetupDefaultBrowserWindow(
        mock_tab_, mock_browser_window_interface_, user_data_host_);
    mock_actor_ui_tab_controller_.emplace(mock_tab_);
  }

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    controller_ = std::make_unique<TestHandoffButtonController>(mock_tab_);

    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    parent_widget_->Show();

    auto widget = std::make_unique<HandoffButtonWidget>();
    auto delegate = std::make_unique<views::WidgetDelegate>();
    auto* button =
        delegate->SetContentsView(std::make_unique<views::LabelButton>());

    views::Widget::InitParams params(
        views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
    params.delegate = delegate.get();
    params.parent = parent_widget_->GetNativeView();
    widget->Init(std::move(params));

    widget_ = widget.get();
    button_ = button;

    controller_->SetWidgetAndButtonForTest(std::move(widget), button_);
    delegate_ = std::move(delegate);
  }

  void SetHoveredCallback(testing::MockFunction<void(bool)>& mock_callback) {
    widget_->SetHoveredCallback(
        base::BindRepeating(&testing::MockFunction<void(bool)>::Call,
                            base::Unretained(&mock_callback)));
  }

  void TearDown() override {
    button_ = nullptr;
    widget_ = nullptr;
    controller_.reset();
    parent_widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  MockActorUiTabController* mock_actor_ui_tab_controller() {
    return &mock_actor_ui_tab_controller_.value();
  }

 protected:
  std::unique_ptr<views::Widget> parent_widget_;
  raw_ptr<HandoffButtonWidget> widget_;
  raw_ptr<views::LabelButton> button_ = nullptr;
  std::unique_ptr<views::WidgetDelegate> delegate_;
  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<TestHandoffButtonController> controller_;
  std::optional<MockActorUiTabController> mock_actor_ui_tab_controller_;
  base::UserActionTester user_action_tester_;
};

TEST_F(HandoffButtonControllerTest,
       ButtonStateUpdatesShouldShowButtonVisibility) {
  HandoffButtonState state;
  state.is_active = true;
  bool should_show = true;

  controller_->UpdateState(state, /*is_visible=*/true);
  controller_->TestShouldShowButton(should_show);
  EXPECT_TRUE(should_show);

  controller_->UpdateState(state, /*is_visible=*/false);
  controller_->TestShouldShowButton(should_show);
  EXPECT_FALSE(should_show);

  state.is_active = false;
  controller_->UpdateState(state, /*is_visible=*/true);
  controller_->TestShouldShowButton(should_show);
  EXPECT_FALSE(should_show);
  EXPECT_EQ(1, controller_->close_button_call_count());

  controller_->UpdateState(state, /*is_visible=*/false);
  controller_->TestShouldShowButton(should_show);
  EXPECT_FALSE(should_show);
  EXPECT_EQ(2, controller_->close_button_call_count());
}

TEST_F(HandoffButtonControllerTest, ButtonTextUpdatesWhenOwnershipChanges) {
  HandoffButtonState state;
  state.is_active = true;
  state.controller = kActor;
  controller_->UpdateState(state, /*is_visible=*/true);
  EXPECT_EQ(button_->GetText(), actor::ui::TAKE_OVER_TASK_TEXT);
  EXPECT_EQ(1, controller_->update_bounds_call_count());
  EXPECT_EQ(1, controller_->update_visibility_call_count());

  state.controller = kClient;
  controller_->UpdateState(state, /*is_visible=*/true);
  EXPECT_EQ(button_->GetText(), actor::ui::GIVE_TASK_BACK_TEXT);
  EXPECT_EQ(2, controller_->update_bounds_call_count());
  EXPECT_EQ(2, controller_->update_visibility_call_count());
}

TEST_F(HandoffButtonControllerTest,
       CallSetActorTaskPausedAndLogMetricsWhenActorHasControlOnButtonPressed) {
  HandoffButtonState actor_state;
  actor_state.is_active = true;
  actor_state.controller = kActor;
  controller_->UpdateState(actor_state, /*is_visible=*/true);

  EXPECT_CALL(*mock_actor_ui_tab_controller(), SetActorTaskPaused());

  controller_->PressButton();

  // Check that the correct user action was recorded
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kActorUiHandoffButtonTakeControlClickedHistogram));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kActorUiHandoffButtonGiveControlClickedHistogram));
}

TEST_F(HandoffButtonControllerTest,
       CallSetActorTaskResumeAndLogMetricsWhenClientHasControlOnButtonPressed) {
  HandoffButtonState client_state;
  client_state.is_active = true;
  client_state.controller = kClient;
  controller_->UpdateState(client_state, /*is_visible=*/true);

  EXPECT_CALL(*mock_actor_ui_tab_controller(), SetActorTaskResume());

  controller_->PressButton();

  // Check that the correct user action was recorded
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kActorUiHandoffButtonGiveControlClickedHistogram));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   kActorUiHandoffButtonTakeControlClickedHistogram));
}

TEST_F(HandoffButtonControllerTest,
       MouseEnteringWidgetFiresHoverCallbackToShowButton) {
  testing::MockFunction<void(bool)> mock_callback;
  SetHoveredCallback(mock_callback);

  EXPECT_CALL(mock_callback, Call(true));

  gfx::Point enter_point =
      widget_->GetContentsView()->GetLocalBounds().CenterPoint();
  ui::MouseEvent mouse_enter_event(ui::EventType::kMouseEntered, enter_point,
                                   enter_point, ui::EventTimeForNow(), 0, 0);

  widget_->OnMouseEvent(&mouse_enter_event);
}

TEST_F(HandoffButtonControllerTest,
       MouseLeavingWidgetFiresHoverCallbackToHideButton) {
  testing::MockFunction<void(bool)> mock_callback;
  SetHoveredCallback(mock_callback);
  // Set widget into a hovered state.
  EXPECT_CALL(mock_callback, Call(true));
  gfx::Point enter_point =
      widget_->GetContentsView()->GetLocalBounds().CenterPoint();
  ui::MouseEvent enter_event(ui::EventType::kMouseEntered, enter_point,
                             enter_point, ui::EventTimeForNow(), 0, 0);
  widget_->OnMouseEvent(&enter_event);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Call(false));

  // Simulate a mouse event far outside the widget's bounds.
  gfx::Point exit_point(-100, -100);
  ui::MouseEvent exit_event(ui::EventType::kMouseExited, exit_point, exit_point,
                            ui::EventTimeForNow(), 0, 0);
  widget_->OnMouseEvent(&exit_event);
}

TEST_F(HandoffButtonControllerTest, HandlesNullTabControllerOnPress) {
  HandoffButtonState actor_state;
  actor_state.is_active = true;
  actor_state.controller = kActor;
  controller_->UpdateState(actor_state, /*is_visible=*/true);
  mock_actor_ui_tab_controller_.reset();
  // Verify that pressing the button does not crash even with a null tab
  // controller.
  controller_->PressButton();
  // The metric is logged outside the null check, so it should still be
  // recorded.
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   kActorUiHandoffButtonTakeControlClickedHistogram));
}

TEST_F(HandoffButtonControllerTest, HandlesNullTabControllerOnHover) {
  mock_actor_ui_tab_controller_.reset();
  // Verify that when the hover status changes to true or false, it does not
  // crash even with a null tab controller.
  controller_->TestUpdateButtonHoverStatus(true);
  controller_->TestUpdateButtonHoverStatus(false);
}

}  // namespace
}  // namespace actor::ui
