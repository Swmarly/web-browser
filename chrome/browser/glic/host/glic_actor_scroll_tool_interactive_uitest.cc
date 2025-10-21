// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/actor/actor_constants.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorScrollToolUiTest : public GlicActorUiTest {
 public:
  MultiStep ScrollAction(std::optional<std::string_view> label,
                         float offset_x,
                         float offset_y,
                         actor::TaskId& task_id,
                         tabs::TabHandle& tab_handle,
                         ExpectedErrorResult expected_result = {});

  MultiStep ScrollAction(std::optional<std::string_view> label,
                         float offset_x,
                         float offset_y,
                         ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorScrollToolUiTest::ScrollAction(
    std::optional<std::string_view> label,
    float offset_x,
    float offset_y,
    actor::TaskId& task_id,
    tabs::TabHandle& tab_handle,
    ExpectedErrorResult expected_result) {
  auto scroll_provider = base::BindLambdaForTesting(
      [this, &task_id, &tab_handle, label, offset_x, offset_y]() {
        std::optional<int32_t> node_id;
        if (label) {
          node_id = SearchAnnotatedPageContent(*label);
        }
        content::RenderFrameHost* frame =
            tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
        Actions action = actor::MakeScroll(*frame, node_id, offset_x, offset_y);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(scroll_provider), std::move(expected_result));
}

MultiStep GlicActorScrollToolUiTest::ScrollAction(
    std::optional<std::string_view> label,
    float offset_x,
    float offset_y,
    ExpectedErrorResult expected_result) {
  return ScrollAction(label, offset_x, offset_y, task_id_, tab_handle_,
                      std::move(expected_result));
}

// Test scrolling the viewport vertically.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollPageVertical) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(/*label=*/std::nullopt, /*offset_x=*/0, kScrollOffsetY),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", kScrollOffsetY));
}

// Test scrolling the viewport horizontally.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollPageHorizontal) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetX = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(/*label=*/std::nullopt, kScrollOffsetX, /*offset_y=*/0),
      WaitForJsResult(kNewActorTabId, "() => window.scrollX", kScrollOffsetX));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, FailOnInvalidNodeId) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(
          base::BindLambdaForTesting([this]() {
            content::RenderFrameHost* frame =
                tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
            Actions action =
                actor::MakeScroll(*frame, kNonExistentContentNodeId,
                                  /*scroll_offset_x=*/0, kScrollOffsetY);
            action.set_task_id(task_id_.value());
            return EncodeActionProto(action);
          }),
          actor::mojom::ActionResultCode::kInvalidDomNodeId),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", 0));
}

// Test scrolling in a sub-scroller on the page.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollElement) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "scroller";
  const int kScrollOffsetY = 50;
  const int kScrollOffsetX = 20;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('scroller').scrollTop",
                      kScrollOffsetY),
      ScrollAction(kElementLabel, kScrollOffsetX, /*offset_y=*/0),
      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('scroller').scrollLeft",
                      kScrollOffsetX));
}

// Test scrolling over a non-scrollable element returns failure.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ScrollNonScrollable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "nonscroll";
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(
          kElementLabel, /*offset_x=*/0, kScrollOffsetY,
          actor::mojom::ActionResultCode::kScrollTargetNotUserScrollable),
      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('nonscroll').scrollTop",
                      /*value=*/0),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", /*value=*/0));
}

// Test scrolling a scroller that's currently offscreen. It will first be
// scrolled into view then scroll applied.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, OffscreenScrollable) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "offscreenscroller";
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY == 0 }"),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      WaitForJsResult(
          kNewActorTabId,
          "() => document.getElementById('offscreenscroller').scrollTop",
          kScrollOffsetY),
      WaitForJsResult(kNewActorTabId, "()=>{ return window.scrollY > 0 }"));
}

// Test that a scrolling over a scroller with overflow in one axis only works
// correctly.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, OneAxisScroller) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "horizontalscroller";
  const int kScrollOffset = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(
          kElementLabel, /*offset_x=*/0, kScrollOffset,
          actor::mojom::ActionResultCode::kScrollTargetNotUserScrollable),
      WaitForJsResult(
          kNewActorTabId,
          "() => document.getElementById('horizontalscroller').scrollTop",
          /*value=*/0),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", /*value=*/0),
      ScrollAction(kElementLabel, kScrollOffset, /*offset_y=*/0),
      WaitForJsResult(
          kNewActorTabId,
          "() => document.getElementById('horizontalscroller').scrollLeft",
          kScrollOffset));
}

// Ensure scroll distances are correctly scaled when browser zoom is applied.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, BrowserZoom) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "scroller";

  double level = blink::ZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  // 60 physical pixels translates to 40 CSS pixels when the zoom factor is 1.5
  // (3 physical pixels : 2 CSS Pixels)
  const int kScrollOffsetPhysical = 60;
  const int kExpectedOffsetCss = 40;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetPhysical),
      WaitForJsResult(kNewActorTabId,
                      "() => document.getElementById('scroller').scrollTop",
                      kExpectedOffsetCss));
}

// Ensure scroll distances are correctly scaled when applied to a CSS zoomed
// scroller.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, CssZoom) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "zoomedscroller";

  // 60 physical pixels translates to 120 CSS pixels since the scroller is
  // inside a `zoom:0.5` subtree (1 physical pixels : 2 CSS Pixels)
  const int kScrollOffsetPhysical = 60;
  const int kExpectedOffsetCss = 120;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetPhysical),
      WaitForJsResult(
          kNewActorTabId,
          "() => document.getElementById('zoomedscroller').scrollTop",
          kExpectedOffsetCss));
}

// Test that a scroll on a page with scroll-behavior:smooth returns success if
// an animation was started, even though it may not have instantly scrolled.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, SmoothScrollSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "smoothscroller";
  const int kScrollOffsetY = 100;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY),
      WaitForJsResult(
          kNewActorTabId,
          "() => document.getElementById('smoothscroller').scrollTop",
          kScrollOffsetY));
}

// Test that a scroll on a page with scroll-behavior:smooth returns failure if
// trying to scroll in a direction with no scrollable extent.
IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, SmoothScrollAtExtent) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  const std::string kElementLabel = "smoothscroller";
  const int kScrollOffsetY = 100;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteJs(kNewActorTabId,
                "() => { "
                "document.querySelector('#smoothscroller').scrollTo({top:"
                "10000, behavior:'instant'})"
                "}"),
      ScrollAction(kElementLabel, /*offset_x=*/0, kScrollOffsetY,
                   actor::mojom::ActionResultCode::kScrollOffsetDidNotChange));
}

IN_PROC_BROWSER_TEST_F(GlicActorScrollToolUiTest, ZeroIdTargetsViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  // DOMNodeIDs start at 1 so 0 should be interpreted as viewport.
  const int kTargetViewport = actor::kRootElementDomNodeId;
  const int kScrollOffsetY = 50;

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      ExecuteAction(base::BindLambdaForTesting([this, kTargetViewport]() {
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
        Actions action =
            actor::MakeScroll(*frame, kTargetViewport,
                              /*scroll_offset_x=*/0, kScrollOffsetY);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      })),
      WaitForJsResult(kNewActorTabId, "() => window.scrollY", kScrollOffsetY));
}

}  // namespace

}  // namespace glic::test
