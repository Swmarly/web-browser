// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;
using content::GetDOMNodeId;

namespace actor {

namespace {

class ActorPageToolBrowserTest : public ActorToolsGeneralPageStabilityTest {
 public:
  ActorPageToolBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor, {{"glic-actor-page-tool-timeout", "100ms"}});
  }
  ~ActorPageToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorPageToolBrowserTest,
    testing::ValuesIn(kActorGeneralPageStabilityModeValues),
    ActorToolsGeneralPageStabilityTest::DescribeParam);

IN_PROC_BROWSER_TEST_P(ActorPageToolBrowserTest, Timeout) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Create a very long string (10MB) to type. This should take much longer
  // than the 100ms timeout and ensure the test doesn't pass without the fix.
  std::string long_string(10 * 1024 * 1024, 'a');

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  std::unique_ptr<ToolRequest> action =
      MakeTypeRequest(*main_frame(), input_id.value(), long_string,
                      /*follow_by_enter=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kToolTimeout);
}

}  // namespace

}  // namespace actor
