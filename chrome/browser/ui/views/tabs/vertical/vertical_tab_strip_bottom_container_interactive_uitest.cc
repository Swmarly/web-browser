// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace base::test {

class VerticalTabStripBottomContainerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  VerticalTabStripBottomContainerInteractiveUiTest() = default;
  ~VerticalTabStripBottomContainerInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);

    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that we can click the new tab button in the bottom container
// of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyNewTabButton) {
  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();

  int tabs_before_click = browser()->tab_strip_model()->GetTabCount();

  RunTestSequence(
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->GetTabCount(); },
          tabs_before_click + 1));
}

}  // namespace base::test
