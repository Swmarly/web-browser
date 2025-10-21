// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point_conversions.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;
using content::RenderFrameHost;
using content::WebContents;

namespace actor {

namespace {

class ActorToolAgnosticBrowserTest : public ActorToolsTest {
 public:
  ActorToolAgnosticBrowserTest() = default;
  ~ActorToolAgnosticBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// Test that requesting tool use on a page that's not active fails. In this case
// we use BFCache but a prerendered page would be another example of an inactive
// page with a live RenderFrameHost.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       InvokeToolInInactiveFrame) {
  // This test relies on BFCache so don't run it if it's not available.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    GTEST_SKIP();
  }

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));

  content::WeakDocumentPtr first_rfh = main_frame()->GetWeakDocumentPtr();
  ASSERT_TRUE(first_rfh.AsRenderFrameHostIfValid()->IsActive());

  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  // Create an action that targets the first document.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*first_rfh.AsRenderFrameHostIfValid(), body_id.value());

  // Navigate to the second document - we expect this should put the first
  // document into the BFCache rather than destroying the RenderFrameHost.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));
  ASSERT_TRUE(first_rfh.AsRenderFrameHostIfValid());
  EXPECT_EQ(first_rfh.AsRenderFrameHostIfValid()->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);
}

// Ensure actuation for a page tool simulates the page having focus. This is
// important to ensure, e.g. 'focus' events are fired on the page in a way that
// matches if a real user was interacting with the page.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       EnsureFocusSimulatedWhenActing) {
  const GURL url_background =
      embedded_test_server()->GetURL("/actor/focus.html");
  const GURL url_foreground =
      embedded_test_server()->GetURL("/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_background));

  WebContents* background_contents = web_contents();

  NavigateParams params(browser(), url_foreground, ::ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ::ui_test_utils::NavigateToURL(&params);

  WebContents* foreground_contents = web_contents();
  ASSERT_NE(web_contents(), background_contents);
  ASSERT_FALSE(background_contents->GetPrimaryMainFrame()
                   ->GetRenderWidgetHost()
                   ->GetView()
                   ->HasFocus());

  content::RenderFrameHost* background_main_frame =
      background_contents->GetPrimaryMainFrame();
  std::optional<int> input_id = GetDOMNodeId(*background_main_frame, "input");
  ASSERT_TRUE(input_id);

  ASSERT_EQ(false, EvalJs(background_contents, "focus_fired"));

  // Create an action that targets the first document.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*background_main_frame, input_id.value());

  {
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    // We shouldn't have change the active web contents, just renderer focus.
    ASSERT_NE(web_contents(), background_contents);
    ASSERT_EQ(web_contents(), foreground_contents);

    ASSERT_EQ(true, EvalJs(background_contents, "focus_fired"));
    EXPECT_EQ(true, EvalJs(background_contents, "document.hasFocus()"));
    // The foreground tab should still think it has focus.
    EXPECT_EQ(true, EvalJs(foreground_contents, "document.hasFocus()"));
  }

  // Reset the page for the next check
  ASSERT_TRUE(ExecJs(background_contents, "focus_fired = false;"));

  // Check that a second action during this task doesn't get another focus
  // event.
  {
    ActResultFuture result;
    action = MakeClickRequest(*background_main_frame, input_id.value());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(false, EvalJs(background_contents, "focus_fired"));
    EXPECT_EQ(true, EvalJs(background_contents, "document.hasFocus()"));
    // The foreground tab should still think it has focus and is the active web
    // contents.
    EXPECT_EQ(true, EvalJs(foreground_contents, "document.hasFocus()"));
    ASSERT_EQ(web_contents(), foreground_contents);
  }

  actor_task().Stop(true);

  // Now that the actor has stopped, the background should lose focus
  EXPECT_EQ(false, EvalJs(background_contents, "document.hasFocus()"));
  // The foreground tab should still think it has focus and is the active web
  // contents.
  EXPECT_EQ(true, EvalJs(foreground_contents, "document.hasFocus()"));
  ASSERT_EQ(web_contents(), foreground_contents);
}

// Basic test to ensure sending a click to an element in a same-site subframe
// works.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       InvokeToolSameSiteSubframe) {
  const GURL url =
      embedded_https_test_server().GetURL("/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  content::RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_FALSE(subframe->IsCrossProcessSubframe());
  ASSERT_TRUE(subframe);

  // Send a click to the button in the subframe.
  std::optional<int> button_id =
      GetDOMNodeIdFromSubframe(*subframe, "#iframe", "button#clickable");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*subframe, button_id.value());

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

// Basic test to ensure sending a click to a coordinate in cross origin subframe
// works.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       InvokeToolCrossSiteSubframeWithCoordinateTarget) {
  const GURL url = embedded_https_test_server().GetURL(
      "/actor/positioned_iframe_no_scroll.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL cross_origin_iframe_url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(
      NavigateIframeToURL(web_contents(), "iframe", cross_origin_iframe_url));

  content::RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  // Addressing flaky test due to layout shift on the iframe
  ASSERT_TRUE(content::ExecJs(web_contents(), "wait()"));
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());

  ASSERT_EQ(EvalJs(subframe, "button_clicked"), false);
  gfx::Point click_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(subframe, "clickable"));
  gfx::RectF subframe_rect = GetBoundingClientRect(*main_frame(), "#iframe");
  gfx::Point transformed_point = gfx::Point(
      subframe_rect.x() + click_point.x(), subframe_rect.y() + click_point.y());

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*active_tab(), transformed_point);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

// Sending an action to an offscreen element on a page should succeed by
// scrolling it into view first.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest, OffscreenElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(EvalJs(web_contents(), "offscreen_button_clicked"), false);

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(EvalJs(web_contents(), "offscreen_button_clicked"), true);
}

// Same as above but the element is an inline element. (i.e. doesn't have a
// LayoutBox).
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest, OffscreenElementInline) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_EQ(EvalJs(web_contents(), "offscreen_inline_clicked"), false);

  std::optional<int> anchor_id =
      GetDOMNodeId(*main_frame(), "a#offscreenInline");
  ASSERT_TRUE(anchor_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), anchor_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(EvalJs(web_contents(), "offscreen_inline_clicked"), true);
}

// Sending an action to an offscreen coordinate should fail.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest, OffscreenCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  {
    ASSERT_EQ(EvalJs(web_contents(), "offscreen_button_clicked"), false);
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    // The point is offscreen.
    ASSERT_GT(click_point.y(), web_contents()->GetSize().height());

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
    EXPECT_EQ(EvalJs(web_contents(), "offscreen_button_clicked"), false);
  }
}

// Sending an action to a coordinate that's outside the document bounds (i.e.
// cannot be scrolled to) should fail.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest, InvalidCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  {
    ASSERT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);
    // A negative coordinate cannot be scrolled to.
    gfx::Point click_point(-1, -10);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
  }

  {
    ASSERT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);
    // y-coordinate is outside the document bounds.
    gfx::Point click_point(1, 10000000);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
  }
}

// Sending an action to an offscreen element on a page that cannot be scrolled
// should fail.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       OffscreenElementNonScrollablePage) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.documentElement.style.overflow = 'hidden';"));

  // Page starts unscrolled
  ASSERT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementOffscreen);

  EXPECT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);
}

// Sending an action to an offscreen fixed position element should fail.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest, OffscreenFixedElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Page starts unscrolled
  ASSERT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreenfixed");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementOffscreen);

  EXPECT_EQ(EvalJs(web_contents(), "window.scrollY"), 0);
}

IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTest,
                       ToolFailsWhenNodeInteractionPointObscured) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_obscured_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_EQ(EvalJs(web_contents(), "target_button_clicked"), false);
  ASSERT_EQ(EvalJs(web_contents(), "obstruction_button_clicked"), false);
  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "button#target");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(
      result, mojom::ActionResultCode::kTargetNodeInteractionPointObscured);
  EXPECT_EQ(EvalJs(web_contents(), "target_button_clicked"), false);
  EXPECT_EQ(EvalJs(web_contents(), "obstruction_button_clicked"), false);
}

class ActorToolAgnosticBrowserTestWithCustomDelay
    : public ActorToolAgnosticBrowserTest {
 public:
  void SetUp() override {
    // Ensure tool doesn't finish before the tab is closed.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{"glic-actor-page-stability-invoke-callback-delay", "500ms"}});
    ActorToolAgnosticBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Closing a tab before tool finishes should cancel callbacks and not crash.
IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTestWithCustomDelay,
                       CloseTabBeforeToolFinishes) {
  // Use a new tab so closing it later won't trigger destruction of browser
  // (needed for proper test teardown).
  AddBlankTabAndShow(browser());
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  web_contents()->Close();
  // ActorTask::OnTabWillDetach will return kError before renderer tool
  // completes.
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);

  // Continue running so tool finish callback from ToolController can proceed
  // after WebContents closed, it should not crash.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
    run_loop.Run();
  }
}

class ToolInvokeWaiter : public ExecutionEngine::StateObserver {
 public:
  ToolInvokeWaiter(base::OnceClosure callback,
                   ExecutionEngine* execution_engine)
      : callback_(std::move(callback)), execution_engine_(execution_engine) {
    execution_engine_->AddObserver(this);
  }
  ~ToolInvokeWaiter() override { execution_engine_->RemoveObserver(this); }

  // `ExecutionEngine::StateObserver`:
  void OnStateChanged(ExecutionEngine::State old_state,
                      ExecutionEngine::State new_state) override {
    if (new_state == ExecutionEngine::State::kToolInvoke) {
      std::move(callback_).Run();
    }
  }

 private:
  base::OnceClosure callback_;
  const raw_ptr<ExecutionEngine> execution_engine_;
};

IN_PROC_BROWSER_TEST_F(ActorToolAgnosticBrowserTestWithCustomDelay,
                       RendererCrashesBeforeToolFinishes) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  base::test::TestFuture<void> tool_invoke_future;
  ToolInvokeWaiter waiter(tool_invoke_future.GetCallback(),
                          actor_task().GetExecutionEngine());
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ASSERT_TRUE(tool_invoke_future.Wait());

  // Crash the renderer.
  {
    content::RenderFrameHostWrapper crashed(
        web_contents()->GetPrimaryMainFrame());
    content::RenderProcessHostWatcher crashed_obs(
        crashed->GetProcess(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    crashed->GetProcess()->Shutdown(content::RESULT_CODE_KILLED);
    crashed_obs.Wait();
    ASSERT_TRUE(crashed.WaitUntilRenderFrameDeleted());
    ASSERT_FALSE(crashed->IsRenderFrameLive());
    ASSERT_FALSE(crashed->GetView());
  }

  ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);

  // Finish the callback from ToolController. No crashes.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
    run_loop.Run();
  }
}

}  // namespace
}  // namespace actor
