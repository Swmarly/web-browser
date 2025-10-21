// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

// This file runs the respective JS tests from
// chrome/test/data/webui/glic/browser_tests/glic_api_browsertest.ts.

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define SLOW_BINARY
#endif

// This skips a test for the multi-instance variant.
#define SKIP_TEST_FOR_MULTI_INSTANCE()                        \
  do {                                                        \
    if (GetParam().multi_instance) {                          \
      GTEST_SKIP() << "Not supported in multi-instance mode"; \
      return;                                                 \
    }                                                         \
  } while (0)

// This skips a test for the multi-instance variant. It's a marker to remember
// to revisit this test later.
#define TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST() SKIP_TEST_FOR_MULTI_INSTANCE()

namespace glic {
namespace {
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
std::vector<std::string> GetTestSuiteNames() {
  return {
      "GlicApiTest",
      "GlicApiTestWithOneTab",
      "GlicApiTestWithFastTimeout",
      "GlicApiTestSystemSettingsTest",
      "GlicApiTestWithOneTabAndContextualCueing",
      "GlicApiTestWithOneTabAndPreloading",
      "GlicApiTestUserStatusCheckTest",
      "GlicApiTestWithOneTabMoreDebounceDelay",
      "GlicGetHostCapabilityApiTest",
      "GlicApiTestWithDefaultTabContextDisabled",
      "GlicApiTestWithDefaultTabContextEnabled",
      "GlicApiTestWithMqlsIdGetterEnabled",
      "GlicApiTestWithMqlsIdGetterDisabled",
      "GlicApiTestRuntimeFeatureOff",
  };
}

// All tests in this file use the same test params here.
struct TestParams {
  bool multi_instance = false;
  // This is only used by one fixture.
  bool enable_scroll_to_pdf = false;
};

class WithTestParams : public testing::WithParamInterface<TestParams> {
 public:
  WithTestParams() {
    if (GetParam().multi_instance) {
      test_param_features_.InitAndEnableFeature(features::kGlicMultiInstance);
    }
  }

  static std::string PrintTestVariant(
      const ::testing::TestParamInfo<TestParams>& info) {
    std::vector<std::string> result;
    if (info.param.enable_scroll_to_pdf) {
      result.push_back("EnableScrollToPdf");
    }
    if (info.param.multi_instance) {
      result.push_back("MultiInst");
    }
    if (result.empty()) {
      return "Default";
    }
    return base::JoinString(result, "_");
  }

 private:
  base::test::ScopedFeatureList test_param_features_;
};

class GlicApiTest : public NonInteractiveGlicApiTest, public WithTestParams {
 public:
  GlicApiTest() : NonInteractiveGlicApiTest("./glic_api_browsertest.js") {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlicScrollTo, {}},
            {features::kGlicClosedCaptioning, {}},
            {features::kGlicApiActivationGating, {}},
            {mojom::features::kGlicMultiTab, {}},
            {features::kGlicWebActuationSetting, {}},
            {features::kGlicUserStatusCheck,
             {{features::kGlicUserStatusRefreshApi.name, "true"},
              {features::kGlicUserStatusThrottleInterval.name, "2s"}}},
        },
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });
  }

  void SetUpOnMainThread() override {
    NonInteractiveGlicApiTest::SetUpOnMainThread();

    histogram_tester = std::make_unique<base::HistogramTester>();
    user_action_tester = std::make_unique<base::UserActionTester>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(b/447705905): Remove extra logging for debugging.
    vmodule_switches_.InitWithSwitches("glic_focused_browser_manager=1");
    NonInteractiveGlicApiTest::SetUpCommandLine(command_line);
  }

  // Common setup used in some tests.
  void NavigateTabAndOpenGlic(bool open_floating = false) {
    if (open_floating) {
      TrackFloatingGlicInstance();
    }
    // Load the test page in a tab, so that there is some page context.
    RunTestSequence(
        InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, page_url()),
        Log("Opening Glic window"),
        !open_floating
            ? OpenGlicWindow(GlicWindowMode::kDetached,
                             GlicInstrumentMode::kHostAndContents)
            : OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents),
        Log("Done opening glic window"));
  }

  void NavigateTabAndOpenGlicFloating() { NavigateTabAndOpenGlic(true); }

  GURL page_url() {
    return InProcessBrowserTest::embedded_test_server()->GetURL(
        "/glic/browser_tests/test.html");
  }

  std::unique_ptr<base::HistogramTester> histogram_tester;
  std::unique_ptr<base::UserActionTester> user_action_tester;

 protected:
  base::test::ScopedFeatureList features_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
};

class GlicApiTestWithOneTab : public GlicApiTest {
 public:
  GlicApiTestWithOneTab() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlicClosedCaptioning},
        /*disabled_features=*/
        {});
  }

  void SetUpOnMainThread() override {
    GlicApiTest::SetUpOnMainThread();

    NavigateTabAndOpenGlic();
  }

  std::string GetDocumentIdForTab(ui::ElementIdentifier tab_id) {
    ui::TrackedElement* const element =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(tab_id);
    CHECK(element);
    content::RenderFrameHost* rfh = AsInstrumentedWebContents(element)
                                        ->web_contents()
                                        ->GetPrimaryMainFrame();
    return optimization_guide::DocumentIdentifierUserData::
        GetDocumentIdentifier(rfh->GetGlobalFrameToken())
            .value();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicApiTestWithDefaultTabContextEnabled : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithDefaultTabContextEnabled() {
    feature_list_.InitWithFeatures({features::kGlicDefaultTabContextSetting},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestWithDefaultTabContextDisabled : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithDefaultTabContextDisabled() {
    feature_list_.InitWithFeatures({},
                                   {features::kGlicDefaultTabContextSetting});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class GlicApiTestWithMqlsIdGetterEnabled : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithMqlsIdGetterEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {mojom::features::kGlicAppendModelQualityClientId},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicApiTestWithMqlsIdGetterDisabled : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithMqlsIdGetterDisabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {mojom::features::kGlicAppendModelQualityClientId});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test fixture that preloads the web client before starting the test.
class GlicApiTestWithOneTabAndPreloading : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithOneTabAndPreloading() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic,
          {
              {"glic-default-hotkey", "Ctrl+G"},
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {features::kGlicApiActivationGating, {}},
         {features::kGlicWarming,
          {{features::kGlicWarmingDelayMs.name, "0"},
           {features::kGlicWarmingJitterMs.name, "0"}}}},
        /*disabled_features=*/
        {});
    // This will temporarily disable preloading to ensure that we don't load
    // the web client before we've initialized the embedded test server and
    // can set the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    GlicProfileManager::ForceConnectionTypeForTesting(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
  }

  auto CreateAndWarmGlic() {
    return Do([this] { GetService()->TryPreload(); });
  }

  auto ResetMemoryPressure() {
    return Do([]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MEMORY_PRESSURE_LEVEL_NONE);
    });
  }

  void SetUpOnMainThread() override {
    // GlicApiTestWithOneTab::SetUpOnMainThread also opens the glic panel, so
    // duplicate everything else it does and call
    // GlicApiTest::SetUpOnMainThread directly.
    GlicApiTest::SetUpOnMainThread();
    histogram_tester = std::make_unique<base::HistogramTester>();
    RunTestSequence(InstrumentTab(kFirstTab),
                    NavigateWebContents(kFirstTab, page_url()));

    // Preload the web client.
    RunTestSequence(WaitForShow(kGlicButtonElementId), ResetMemoryPressure(),
                    ObserveState(glic::test::internal::kWebUiState, GetHost()),
                    CreateAndWarmGlic(),
                    WaitForState(glic::test::internal::kWebUiState,
                                 mojom::WebUiState::kReady),
                    CheckControllerShowing(false));
  }

  void TearDown() override {
    GlicApiTestWithOneTab::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
  }

 private:
  base::test::ScopedFeatureList features_;
};

class GlicApiTestWithOneTabAndContextualCueing : public GlicApiTestWithOneTab {
 public:
  GlicApiTestWithOneTabAndContextualCueing() {
    contextual_cueing_features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic,
          {
              {"glic-default-hotkey", "Ctrl+G"},
              // Shorten load timeouts.
              {features::kGlicPreLoadingTimeMs.name, "20"},
              {features::kGlicMinLoadingTimeMs.name, "40"},
          }},
         {features::kGlicApiActivationGating, {}},
         {contextual_cueing::kGlicZeroStateSuggestions, {}},
         {mojom::features::kZeroStateSuggestionsV2, {}}},
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });
  }
  // Create the mock service.
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
    mock_cueing_service_ = static_cast<
        testing::NiceMock<contextual_cueing::MockContextualCueingService>*>(
        contextual_cueing::ContextualCueingServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser_context,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<testing::NiceMock<
                      contextual_cueing::MockContextualCueingService>>();
                })));

    GlicApiTestWithOneTab::SetUpBrowserContextKeyedServices(browser_context);
  }

  void TearDownOnMainThread() override {
    mock_cueing_service_ = nullptr;
    GlicApiTestWithOneTab::TearDownOnMainThread();
  }

  contextual_cueing::MockContextualCueingService* mock_cueing_service() {
    return mock_cueing_service_.get();
  }

 private:
  raw_ptr<testing::NiceMock<contextual_cueing::MockContextualCueingService>>
      mock_cueing_service_;
  base::test::ScopedFeatureList contextual_cueing_features_;
};

class GlicApiTestWithFastTimeout : public GlicApiTest {
 public:
  GlicApiTestWithFastTimeout() {
    features2_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{
            features::kGlic,
            {
// For slow binaries, use a longer timeout.
#if defined(SLOW_BINARY)
                {features::kGlicMaxLoadingTimeMs.name, "6000"},
#else
                {features::kGlicMaxLoadingTimeMs.name, "3000"},
#endif
            },
        }},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList features2_;
};

// Note: Test names must match test function names in api_test.ts.

// TODO(harringtond): Many of these tests are minimal, and could be improved
// with additional cases and additional assertions.

// Just verify the test harness works.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testDoNothing) {
  ExecuteJsTest();
}

// Confirms that JS assertion errors captured by try-catch blocks will still
// result in test failures.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testFailureForCapturedApiTestError) {
  const std::string expected_failure =
      "Failed at step #1 (single or first) due to (captured error): "
      "Error: Non-throwing test error";
  ExecuteJsTest(
      {.should_fail = true, .should_fail_with_error = expected_failure});
}

// Checks that all tests in api_test.ts have a corresponding test case in this
// file.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testAllTestsAreRegistered) {
  AssertAllTestsRegistered(GetTestSuiteNames());
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testLoadWhileWindowClosed) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone),
      CloseGlic());
  ExecuteJsTest();
  // Make sure the WebUI transitions to kReady, otherwise the web client may be
  // destroyed.
  WaitForWebUiState(mojom::WebUiState::kReady);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFailsWindowClosed) {
  base::HistogramTester histogram_tester;
  // Immediately close the window to check behavior while window is closed.
  // Fail client initialization, should see error page.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone),
      CloseGlic());
  ExecuteJsTest();
  WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester.ExpectUniqueSample(
      "Glic.Host.WebClientState.OnDestroy",
      /*sample=*/2 /*WEB_CLIENT_INITIALIZE_FAILED*/, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFailsWindowOpen) {
  // Fail client initialization, should see error page.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "error")),
  });
  WaitForWebUiState(mojom::WebUiState::kError);

  // Closing and reopening the window should trigger a retry. This time the
  // client initializes correctly.
  RunTestSequence(CloseGlic(), OpenGlicWindow(GlicWindowMode::kDetached,
                                              GlicInstrumentMode::kNone));
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
  WaitForWebUiState(mojom::WebUiState::kReady);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextDisabled,
                       testDefaultTabContextApiIsUndefinedWhenFeatureDisabled) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithDefaultTabContextEnabled,
                       testGetDefaultTabContextPermissionState) {
  // Default kGlicDefaultTabContextEnabled value is true.
  ExecuteJsTest();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kGlicDefaultTabContextEnabled, false);
  ContinueJsTest();
}

// TODO(crbug.com/409042450): This is a flaky on MSAN.
#if defined(SLOW_BINARY)
#define MAYBE_testReload DISABLED_testReload
#else
#define MAYBE_testReload testReload
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTest, MAYBE_testReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest({
      .params = base::Value(
          base::Value::Dict().Set("failWith", "reloadAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testReloadWebUi) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest();

  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ReloadGlicWebui();
  listener.WaitForWebUiState(mojom::WebUiState::kUninitialized);
  ExecuteJsTest();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetHost()->GetPageHandlersForTesting().size() == 1; }));
  // Reloading the WebUI should trigger loading a second page handler.
  // That page handler should become the primary page handler.
  // This assertion is a regression test for b/418258791.
  ASSERT_TRUE(GetHost()->GetPrimaryPageHandlerForTesting());
}

// The client navigates to the 'sorry' page before it finishes initialize().
// Chrome should show this page.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testSorryPageBeforeInitialize) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set(
          "failWith", "navigateToSorryPageBeforeInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kGuestError);
  RunTestSequence(CheckControllerShowing(true));

  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(FindGlicGuestMainFrame(),
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testSorryPageAfterInitialize) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set(
          "failWith", "navigateToSorryPageAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kGuestError);
  RunTestSequence(CheckControllerShowing(true));

  // Simulate completing a captcha, navigating back.
  ASSERT_EQ(true,
            content::EvalJs(FindGlicGuestMainFrame(),
                            std::string("(()=>{window.location = '") +
                                GetGuestURL().spec() + "'; return true;})()"));

  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "none")),
  });
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitializeFailsAfterReload) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest({
      .params = base::Value(
          base::Value::Dict().Set("failWith", "reloadAfterInitialize")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "error")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kError);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, testNoClientCreated) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  // Note that the client does receive the bootstrap message, but never calls
  // back, so from the host's perspective bootstrapping is still pending.
  // There may be warmed instances that also receive this error, so expect at
  // least one count.
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Glic.Host.WebClientState.OnDestroy", 0 /*BOOTSTRAP_PENDING*/),
            0);
#endif
}

// In this test, the client page does not initiate the bootstrap process, so no
// client connects.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, testNoBootstrap) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  // May have more than one sample because there can be a warmed instance.
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Glic.Host.WebClientState.OnDestroy", 0 /*BOOTSTRAP_PENDING*/),
            0);
#endif
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, testInitializeTimesOut) {
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  WebUIStateListener listener(GetHost());
  ExecuteJsTest({
      .params = base::Value(base::Value::Dict().Set("failWith", "timeout")),
  });
  listener.WaitForWebUiState(mojom::WebUiState::kError);
  // There may be warmed instances that also receive this error, so expect at
  // least one count.
  EXPECT_GT(
      histogram_tester.GetBucketCount("Glic.Host.WebClientState.OnDestroy",
                                      3 /*WEB_CLIENT_NOT_INITIALIZED*/),
      0);
#endif
}

// Connect the client, and check that the special request header is sent.
IN_PROC_BROWSER_TEST_P(GlicApiTest, testRequestHeader) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  const GURL cross_origin_rpc_url =
      embedded_test_server()->GetURL("b.com", "/fake-rpc/cors");
  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "rpcUrls", base::Value::List()
                                    .Append("/fake-rpc")
                                    .Append(cross_origin_rpc_url.spec())))});

  auto request_header_matcher = testing::AllOf(
      Contains(Pair("x-glic", "1")),
      Contains(
          Pair("x-glic-chrome-channel",
               testing::AnyOf("unknown", "canary", "dev", "beta", "stable"))),
      Contains(
          Pair("x-glic-chrome-version", version_info::GetVersionNumber())));

  auto find_request = [&](std::string_view path) {
    const auto it = std::ranges::find_if(
        embedded_test_server_requests_, [&](const auto& request) {
          return request.GetURL().GetPath() == path &&
                 request.method == net::test_server::METHOD_GET;
        });
    return it == embedded_test_server_requests_.end() ? nullptr : &(*it);
  };

  auto* main_request = find_request(GetGuestURL().GetPath());
  ASSERT_TRUE(main_request);
  EXPECT_THAT(main_request->headers, request_header_matcher);

  auto* rpc_request = find_request("/fake-rpc");
  ASSERT_TRUE(rpc_request);
  EXPECT_THAT(rpc_request->headers, request_header_matcher);

  auto* cross_origin_rpc_request = find_request("/fake-rpc/cors");
  ASSERT_TRUE(cross_origin_rpc_request);
  EXPECT_THAT(cross_origin_rpc_request->headers, request_header_matcher);
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTab) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(2));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabFailsWithUnsupportedScheme) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(1));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabInBackground) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));

  // Creating a new tab via the glic API in the foreground should change the
  // active tab.
  ExecuteJsTest();
  RunTestSequence(CheckTabCount(2));
  tabs::TabInterface* active_tab =
      InProcessBrowserTest::browser()->tab_strip_model()->GetActiveTab();
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));

  // Creating a new tab via the glic API in the background should not change the
  // active tab.
  ContinueJsTest();
  RunTestSequence(CheckTabCount(3));
  active_tab =
      InProcessBrowserTest::browser()->tab_strip_model()->GetActiveTab();
  ASSERT_THAT(active_tab->GetContents()->GetURL().spec(),
              testing::EndsWith("#foreground"));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabByClickingOnLink) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));
  // Have the test track this tab's glic instance.
  TrackGlicInstanceWithId(GetGlicInstance()->id());
  content::RenderFrameHost* guest_frame = FindGlicGuestMainFrame();
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return InProcessBrowserTest::browser()->tab_strip_model()->GetTabCount() ==
           2;
  })) << "Timed out waiting for tab count to increase. Tab count = "
      << InProcessBrowserTest::browser()->tab_strip_model()->GetTabCount();
  // The guest frame shouldn't change.
  ASSERT_EQ(guest_frame, FindGlicGuestMainFrame());

  // This test is a regression test for b/416464184.
  // Audio ducking should still work after clicking a link.
  AudioDucker* audio_ducker =
      AudioDucker::GetForPage(FindGlicGuestMainFrame()->GetPage());
  ASSERT_TRUE(audio_ducker);
  ASSERT_EQ(audio_ducker->GetAudioDuckingState(),
            AudioDucker::AudioDuckingState::kDucking);

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return audio_ducker->GetAudioDuckingState() ==
           AudioDucker::AudioDuckingState::kNoDucking;
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabByClickingOnLinkDaisyChains) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Test only supported with multi-instance on";
  }

  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckTabCount(1));

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCreateTabFailsIfNotActive) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testOpenGlicSettingsPage) {
  ExecuteJsTest();

  RunTestSequence(
      InstrumentTab(kSettingsTab),
      WaitForWebContentsReady(
          kSettingsTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPanelStateAttached) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Attached only supported with multi-instance.";
  }
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPanelStateAttachedHidden) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Attached only supported with multi-instance.";
  }
  ExecuteJsTest();

  // Open and select a second tab. This should result in panel state hidden.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  ContinueJsTest();

  // Open the first tab again, it should send the attached state.
  browser()->tab_strip_model()->ActivateTabAt(0);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testDetachPanel) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Attached only supported with multi-instance.";
  }
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testMultiplePanelsDetachedAndFloating) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Attached only supported with multi-instance.";
  }
  // Open two tabs, select the first, open glic.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()));

  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));

  // Execute test on the first tab instance.
  ExecuteJsTest({.params = base::Value("first")});

  // Select the second tab, open glic, and execute the test on the second
  // instance.
  TrackGlicInstanceWithTabIndex(1);
  browser()->tab_strip_model()->ActivateTabAt(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value("second")});

  // Continue on the first tab.
  TrackGlicInstanceWithTabIndex(0);
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testThereCanOnlyBeOneFloaty) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Attached only supported with multi-instance.";
  }
  // Open two tabs, select the first, open Floaty glic.
  RunTestSequence(InstrumentTab(kFirstTab),
                  NavigateWebContents(kFirstTab, page_url()));

  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  GlicInstanceImpl* tab0_instance = GetGlicInstanceImpl();
  // Execute test on the first tab instance.
  ExecuteJsTest({.params = base::Value("first")});
  ASSERT_EQ(mojom::PanelStateKind::kDetached,
            tab0_instance->GetPanelState().kind);

  // Select the second tab, open Floaty, and execute the test on the second
  // instance.
  TrackGlicInstanceWithTabIndex(1);
  browser()->tab_strip_model()->ActivateTabAt(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value("second")});
  GlicInstanceImpl* tab1_instance = GetGlicInstanceImpl();

  // Continue on the first tab. Verify there's only one Floaty.
  TrackGlicInstanceWithTabIndex(0);
  ContinueJsTest();
  ASSERT_EQ(mojom::PanelStateKind::kDetached,
            tab1_instance->GetPanelState().kind);
  ASSERT_EQ(mojom::PanelStateKind::kHidden,
            tab0_instance->GetPanelState().kind);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testClosePanel) {
  ExecuteJsTest();
  RunTestSequence(WaitForHide(kGlicViewElementId));
}

class GlicApiTestRuntimeFeatureOff : public GlicApiTestWithOneTab {
 public:
  GlicApiTestRuntimeFeatureOff() {
    with_feature_off_.InitAndDisableFeature(
        mojom::features::kGlicAppendModelQualityClientId);
  }

 private:
  base::test::ScopedFeatureList with_feature_off_;
};

// This tests what happens when a mojom RuntimeFeature method is called by
// the host.
// DONT DELETE THIS TEST when the method being called here is removed,
// but instead update this test to call any other RuntimeFeature-protected
// method.
IN_PROC_BROWSER_TEST_P(GlicApiTestRuntimeFeatureOff,
                       testErrorShownOnMojoPipeError) {
  ExecuteJsTest();

  auto* web_contents = FindGlicWebUIContents();
  // Reach in to `GlicApiHost`'s handler to call a function that's gated by
  // a disabled feature.
  const char* script = R"js(
(()=>{
  const appController = appRouter.glicController;
  if (!appController.webview.host.handler.getModelQualityClientId) {
    return "Method not found";
  }
  appController.webview.host.handler.getModelQualityClientId();
  return "Method called";
})()
)js";
  auto result = content::EvalJs(web_contents->GetPrimaryMainFrame(), script);
  ASSERT_EQ("Method called", result.ExtractString());

  WaitForWebUiState(mojom::WebUiState::kError);
  histogram_tester->ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                       9 /*MOJO_PIPE_CLOSED_UNEXPECTEDLY*/, 1);

  // Verify the reload button works.
  RunTestSequence(ExecuteJsAt(
      test::kGlicHostElementId, {"#reload"}, "(el)=>el.click()",
      InteractiveBrowserTestApi::ExecuteJsMode::kWaitForCompletion));

  WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testShowProfilePicker) {
  base::test::TestFuture<void> profile_picker_opened;
  ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
      profile_picker_opened.GetCallback());
  ExecuteJsTest();
  ASSERT_TRUE(profile_picker_opened.Wait());
  // TODO(harringtond): Try to test changing profiles.
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testPanelActive) {
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
  ExecuteJsTest();

  // Opening a new browser window will deactivate the previous one, and make
  // the panel not active.
  NavigateParams params(browser()->profile(), GURL("about:blank"),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testIsBrowserOpen) {
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  TrackGlicInstanceWithId(GetGlicInstance()->id());
  ExecuteJsTest();

  // Open a new incognito tab so that Chrome doesn't exit, and close the first
  // browser.
  CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser());

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testEnableDragResize) {
  // TODO: resize is not yet implemented for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(WaitForCanResizeEnabled(/*enabled=*/true));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testDisableDragResize) {
  // TODO: resize is not yet implemented for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  // Check the default resize setting here.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  WaitForCanResizeEnabled(/*enabled=*/true));
  ExecuteJsTest();
  RunTestSequence(WaitForCanResizeEnabled(/*enabled=*/false));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testInitiallyNotResizable) {
  // TODO: resize is not yet implemented for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();
  RunTestSequence(WaitForCanResizeEnabled(/*enabled=*/false));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithMqlsIdGetterEnabled,
                       testGetModelQualityClientIdFeatureEnabled) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithMqlsIdGetterDisabled,
                       testGetModelQualityClientIdFeatureDisabled) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndContextualCueing,
                       testGetZeroStateSuggestionsForFocusedTabApi) {
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .Times(1);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(
    GlicApiTestWithOneTabAndContextualCueing,
    testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden) {
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .Times(0);

  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndContextualCueing,
                       testGetZeroStateSuggestionsApi) {
  if (GetParam().multi_instance) {
    EXPECT_CALL(
        *mock_cueing_service(),
        GetContextualGlicZeroStateSuggestionsForPinnedTabs(_, _, _, _, _))
        .Times(testing::AtLeast(1));
    // TODO(b/451618836): This is currently called 4 times, but should only be
    // called once.
  } else {
    EXPECT_CALL(*mock_cueing_service(),
                GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
        .Times(1);
  }

  ExecuteJsTest();
}

// TODO(crbug.com/449897870): Flaky on Win-asan.
#if (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_testGetZeroStateSuggestionsMultipleNavigations \
  DISABLED_testGetZeroStateSuggestionsMultipleNavigations
#else
#define MAYBE_testGetZeroStateSuggestionsMultipleNavigations \
  testGetZeroStateSuggestionsMultipleNavigations
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndContextualCueing,
                       MAYBE_testGetZeroStateSuggestionsMultipleNavigations) {
  // TODO: zero state suggestions not yet implemented for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .Times(1);
  ExecuteJsTest();

  // Navigate to another page in the existing tab.
  std::vector<std::string> suggestions = {"suggestion1", "suggestion2",
                                          "suggestion3"};
  // This gets called once for the primary page change and once for the title
  // change. This is fine. In the actual cueing service implementation, it
  // coalesces the calls for the same page if there is already an existing
  // request for the page in flight.
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .WillRepeatedly(RunOnceCallbackRepeatedly<3>(suggestions));
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));

  // Confirm that the observer is notified through getZeroStateSuggestions of
  // the second page navigation.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndContextualCueing,
                       testGetZeroStateSuggestionsFailsWhenHidden) {
  // TODO: zero state suggestions not yet implemented for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  // Initial state.
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .Times(1);
  ExecuteJsTest();

  testing::Mock::VerifyAndClearExpectations(mock_cueing_service());

  // Navigate to another page in the existing tab. Panel should be closed here
  // so should not get suggestions for tab.
  EXPECT_CALL(*mock_cueing_service(),
              GetContextualGlicZeroStateSuggestionsForFocusedTab(_, _, _, _))
      .Times(0);
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));
  ContinueJsTest();
}

// TODO(crbug.com/435271214): Re-enable this test
#if BUILDFLAG(IS_LINUX) || (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_testDeferredFocusedTabStateAtCreation \
  DISABLED_testDeferredFocusedTabStateAtCreation
#else
#define MAYBE_testDeferredFocusedTabStateAtCreation \
  testDeferredFocusedTabStateAtCreation
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndPreloading,
                       MAYBE_testDeferredFocusedTabStateAtCreation) {
  // Navigate the first tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));
  ExecuteJsTest();
  RunTestSequence(ToggleGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerShowing(true));
  ContinueJsTest();
}

// Tests that both focused and arbitrary tab extraction are rejected
// when the glic panel is hidden.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTabAndPreloading,
                       testNoExtractionWhileHidden) {
  // Attempt to extract focused tab context with the preloaded client.
  ExecuteJsTest();

  // TODO(b/450923405): Metrics checks fail on win-rel.
#if !BUILDFLAG(IS_WIN) && !defined(DEBUG)
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 1);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestHandlerException, 1);
  histogram_tester->ExpectTotalCount("Glic.Api.RequestCounts.GetContextFromTab",
                                     0);
#endif

  // Open the glic panel and attempt to extract focused and arbitrary tab
  // context.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ContinueJsTest();
  // TODO(b/450923405): Metrics checks fail on win-rel.
#if !BUILDFLAG(IS_WIN) && !defined(DEBUG)
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 1);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestHandlerException, 1);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 0);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromTab",
      GlicRequestEvent::kRequestHandlerException, 0);
#endif

  // Hide the glic panel again and attempt to extract focused and arbitrary tab
  // context.
  RunTestSequence(CloseGlic());
  ContinueJsTest();
  // TODO(b/450923405): Metrics checks fail on win-rel.
#if !BUILDFLAG(IS_WIN) && !defined(DEBUG)
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 2);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromFocusedTab",
      GlicRequestEvent::kRequestHandlerException, 2);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 1);
  histogram_tester->ExpectBucketCount(
      "Glic.Api.RequestCounts.GetContextFromTab",
      GlicRequestEvent::kRequestHandlerException, 1);
#endif
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetFocusedTabStateV2) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetFocusedTabStateV2WithNavigation) {
  // Confirm that the observer is notified through getFocusedTabState of the
  // initial state, i.e. the first page navigation.
  ExecuteJsTest();

  // Navigate to another page in the existing tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));

  // Confirm that the observer is notified through getFocusedTabState of the
  // second page navigation.
  ContinueJsTest();

  // Open a new tab and navigate to a another page.
  RunTestSequence(AddInstrumentedTabWithOpener(
      kSecondTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                      "/glic/browser_tests/test.html")));

  // Confirm that the observer is notified through getFocusedTabState that due
  // to a page navigation in a new tab, a new tab has gained focus.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetFocusedTabStateV2WithNavigationWhenInactive) {
  SKIP_TEST_FOR_MULTI_INSTANCE();
  TrackGlicInstanceWithId(GetGlicInstance()->id());
  // Confirm that the observer is notified through getFocusedTabState of the
  // initial state, i.e. the first page navigation. It should then hide.
  ExecuteJsTest();

  // Navigate to another page in the existing tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));

  // Open a new tab, navigate to a another page, and open the glic window.
  RunTestSequence(
      AddInstrumentedTab(kSecondTab,
                         InProcessBrowserTest::embedded_test_server()->GetURL(
                             "/glic/browser_tests/test.html")),
      OpenGlicWindow(GlicWindowMode::kDetached,
                     GlicInstrumentMode::kHostAndContents));

  // Confirm that the observer only notified of this last state.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testGetFocusedTabStateV2BrowserClosed) {
  // TODO(harringtond): This test is flaky in multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
  // Note: ideally this test would only open Glic after the main browser is
  // closed. This however crashes in `OpenGlicWindow()`.
  TrackFloatingGlicInstance();
  RunTestSequence(OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents));

  // Open a new incognito window first so that Chrome doesn't exit, then close
  // the first browser window.
  CreateIncognitoBrowser();
  CloseBrowserAsynchronously(browser());

  ExecuteJsTest({.wait_for_guest = false});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithoutPermission) {
  // In multi-instance mode, we only fetch context from pinned tabs.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest();

  // Should record the respective error to the text mode histogram.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromFocusedTab.Error"),
      UnorderedElementsAre(Pair(
          "Glic.Api.GetContextFromFocusedTab.Error.Text",
          BucketsAre(Bucket(GlicGetContextFromTabError::
                                kPermissionDeniedContextPermissionNotEnabled,
                            1)))));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromPinnedTabWithoutPermission) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithNoRequestedData) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

// Win-ASAN is flaky.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  DISABLED_testGetContextFromFocusedTabWithAllRequestedData
#else
#define MAYBE_testGetContextFromFocusedTabWithAllRequestedData \
  testGetContextFromFocusedTabWithAllRequestedData
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testGetContextFromFocusedTabWithAllRequestedData) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextForActorFromTabWithoutPermission) {
  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextForActorFromTab.Error"),
              testing::IsEmpty());
}

#if BUILDFLAG(ENABLE_PDF)
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  testGetContextFromFocusedTabWithPdfFile
#else
#define MAYBE_testGetContextFromFocusedTabWithPdfFile \
  DISABLED_testGetContextFromFocusedTabWithPdfFile
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testGetContextFromFocusedTabWithPdfFile) {
  RunTestSequence(NavigateWebContents(
      kFirstTab,
      InProcessBrowserTest::embedded_test_server()->GetURL("/pdf/test.pdf")));

  ExecuteJsTest();

  // No context error should have been recorded.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromFocusedTabWithUnFocusablePage) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Navigate to an un-focusable internal page.
  RunTestSequence(NavigateWebContents(kFirstTab, chrome::GetSettingsUrl("")));

  // Web client request focused tab contents.
  ExecuteJsTest();

  // Checks that the correct error was reported.
  EXPECT_THAT(histogram_tester->GetAllSamplesForPrefix(
                  "Glic.Api.GetContextFromFocusedTab.Error"),
              UnorderedElementsAre(Pair(
                  "Glic.Api.GetContextFromFocusedTab.Error.Text",
                  BucketsAre(Bucket(
                      GlicGetContextFromTabError::kPermissionDenied, 1)))));
}

// TODO(harringtond): Fix this, it hangs.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, DISABLED_testCaptureScreenshot) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testPermissionAccess) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  ExecuteJsTest();
  histogram_tester->ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnTabContextPermissionGranted",
      ActiveTabSharingState::kActiveTabIsShared, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testClosedCaptioning) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetUserProfileInfo) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetUserProfileInfoDoesNotDeferWhenInactive) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testRefreshSignInCookies) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testActuationOnWebSetting) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSignInPauseState) {
  // Check that Glic web client is open and can retrieve the user's info.
  ExecuteJsTest({.expect_guest_frame_destroyed = false});

  // Pause the sign-in.
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // The guest frame should be destroyed, and the WebUI should show the sign-in
  // panel.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return FindGlicGuestMainFrame() == nullptr; }));
  WaitForWebUiState(mojom::WebUiState::kSignIn);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSetContextAccessIndicator) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSetAudioDucking) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetDisplayMedia) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testJournal) {
  ExecuteJsTest();
}

// TODO(crbug.com/438812885): This is flaky.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, DISABLED_testMetrics) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kGlicClosedCaptioningEnabled, true);

  ExecuteJsTest();
  // Sleeping here is needed so that the calls made from the web client are
  // handled by the browser before the check below.
  sleepWithRunLoop(base::Milliseconds(100));
  histogram_tester->ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnUserInputSubmitted",
      ActiveTabSharingState::kTabContextPermissionNotGranted, 1);

  histogram_tester->ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                       true, 1);
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicContextUploadStarted"));
  EXPECT_EQ(1,
            user_action_tester->GetActionCount("GlicContextUploadCompleted"));
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicReactionModelled"));
  EXPECT_EQ(1, user_action_tester->GetActionCount("GlicResponseStopByUser"));
  histogram_tester->ExpectTotalCount("Glic.FirstReaction.Text.Modelled.Time",
                                     1);
  histogram_tester->ExpectTotalCount("Glic.TabContext.UploadTime", 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testScrollToFindsText) {
  // TODO(b/446757683): GlicAnnotationManager doesn't work for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "documentId", GetDocumentIdForTab(kFirstTab)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testScrollToFindsTextNoTabContextPermission) {
  // TODO(b/446757683): GlicAnnotationManager doesn't work for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "documentId", GetDocumentIdForTab(kFirstTab)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testScrollToFailsWhenInactive) {
  // TODO(b/446757683): GlicAnnotationManager doesn't work for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "documentId", GetDocumentIdForTab(kFirstTab)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testScrollToNoMatchFound) {
  // TODO(b/446757683): GlicAnnotationManager doesn't work for multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "documentId", GetDocumentIdForTab(kFirstTab)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSetSyntheticExperimentState) {
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "Enabled");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testSetSyntheticExperimentStateMultiProfile) {
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([]() {
    std::vector<variations::ActiveGroupId> trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();
    variations::ActiveGroupId expected =
        variations::MakeActiveGroupId("TestTrial", "MultiProfileDetected");
    return std::ranges::any_of(trials, [&](const auto& trial) {
      return trial.name == expected.name && trial.group == expected.group;
    });
  }));
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCloseAndOpenWhileOpening) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testNotifyPanelWillOpenIsCalledOnce) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  ExecuteJsTest();
  histogram_tester->ExpectUniqueSample(
      "Glic.Sharing.ActiveTabSharingState.OnPanelOpenAndReady",
      ActiveTabSharingState::kTabContextPermissionNotGranted, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetOsHotkeyState) {
  ExecuteJsTest();
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey,
                                              "Ctrl+Shift+1");
  ContinueJsTest();
  g_browser_process->local_state()->SetString(prefs::kGlicLauncherHotkey, "");
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSetWindowDraggableAreas) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  ExecuteJsTest();
  const int x = 10;
  const int y = 20;
  const int width = 30;
  const int height = 40;

  RunTestSequence(
      // Test points within the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height - 1), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y + height - 1),
                                      true),
      // Test points at the edges of the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x - 1, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y - 1), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height), false));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testSetWindowDraggableAreasDefault) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // TODO(crbug.com/404845792): Default draggable area is currently hardcoded in
  // glic_page_handler.cc. This should be moved to a shared location and updated
  // here.
  const int x = 0;
  const int y = 0;
  const int width = 400;
  const int height = 80;

  ExecuteJsTest();
  RunTestSequence(
      // Test points within the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height - 1), true),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width - 1, y + height - 1),
                                      true),
      // Test points at the edges of the draggable area.
      CheckPointIsWithinDraggableArea(gfx::Point(x - 1, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y - 1), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x + width, y), false),
      CheckPointIsWithinDraggableArea(gfx::Point(x, y + height), false));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testSetMinimumWidgetSize) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  ExecuteJsTest();
  ASSERT_TRUE(step_data()->is_dict());
  const auto& min_size = step_data()->GetDict();
  const int width = min_size.FindInt("width").value();
  const int height = min_size.FindInt("height").value();

  RunTestSequence(CheckWidgetMinimumSize(gfx::Size(width, height)));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testManualResizeChanged) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  window_controller().GetGlicWidget()->OnNativeWidgetUserResizeStarted();

  // Check that the web client is notified of the beginning of the user
  // initiated resizing event.
  ExecuteJsTest();

  window_controller().GetGlicWidget()->OnNativeWidgetUserResizeEnded();

  // Check that the web client is notified of the ending of the user
  // initiated resizing event.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testResizeWindowTooSmall) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Web client requests the window to be resized to 0x0, bellow the minimum
  // dimensions (see GlicWindowController#GetLastRequestedSizeClamped), so it
  // gets discarded in favor of the initial size.
  gfx::Size expected_size = GlicWidget::GetInitialSize();
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);

  ExecuteJsTest();

  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testResizeWindowTooLarge) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Web client requests the window to be resized to 20000x20000, above the
  // maximum dimensions (see GlicWindowController#GetLastRequestedSizeClamped),
  // so it gets discarded in favor of the max size. This max size is still
  // larger than the display work area so we clamp the dimensions down to fit on
  // screen.
  ExecuteJsTest();
  gfx::Rect display_bounds =
      display::Screen::Get()->GetPrimaryDisplay().work_area();
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();

  ASSERT_TRUE(display_bounds.Contains(final_widget_bounds));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testResizeWindowWithinBounds) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Web client requests the window to be resized to 800x700, which are valid
  // dimensions.
  gfx::Size expected_size = gfx::Size(800, 700);
  ExecuteJsTest(
      {.params = base::Value(base::Value::Dict()
                                 .Set("width", expected_size.width())
                                 .Set("height", expected_size.height()))});
  GlicWidget* glic_widget = window_controller().GetGlicWidget();
  ASSERT_TRUE(glic_widget);
  gfx::Rect final_widget_bounds = glic_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(expected_size,
            glic_widget->WidgetToVisibleBounds(final_widget_bounds).size());
}

class GlicApiTestSystemSettingsTest : public GlicApiTestWithOneTab {
 public:
  GlicApiTestSystemSettingsTest() {
    system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
  }

  ~GlicApiTestSystemSettingsTest() override {
    system_permission_settings::SetInstanceForTesting(nullptr);
  }

  testing::NiceMock<system_permission_settings::MockPlatformHandle>
      mock_platform_handle;
};

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testOpenOsMediaPermissionSettings) {
  base::test::TestFuture<void> signal;
  EXPECT_CALL(
      mock_platform_handle,
      OpenSystemSettings(testing::_, ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'media'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testOpenOsGeoPermissionSettings) {
  base::test::TestFuture<void> signal;
  EXPECT_CALL(mock_platform_handle,
              OpenSystemSettings(testing::_, ContentSettingsType::GEOLOCATION))
      .WillOnce(base::test::InvokeFuture(signal));

  // Trigger the openOsPermissionSettingsMenu API with 'geolocation'.
  ExecuteJsTest();
  // Wait for OpenSystemSettings to be called.
  EXPECT_TRUE(signal.Wait());
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(testing::Return(true));

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // true as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestSystemSettingsTest,
                       testGetOsMicrophonePermissionStatusNotAllowed) {
  EXPECT_CALL(mock_platform_handle,
              IsAllowed(ContentSettingsType::MEDIASTREAM_MIC))
      .WillOnce(testing::Return(false));

  // Trigger the GetOsMicrophonePermissionStatus API and check if it returns
  // false as mocked by this test.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testNavigateToDifferentClientPage) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  base::HistogramTester histogram_tester;
  WebUIStateListener listener(GetHost());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});  // test run count: 0.
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(1)});  // test run count: 1.
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnCommit",
                                      6 /*RESPONSIVE*/, 1);
  histogram_tester.ExpectUniqueSample("Glic.Host.WebClientState.OnDestroy",
                                      0 /*BOOTSTRAP_PENDING*/, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout, testNavigateToAboutBlank) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  WebUIStateListener listener(GetHost());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest();
  listener.WaitForWebUiState(mojom::WebUiState::kError);
}

// TODO(crbug.com/410881522): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_testNavigateToBadPage DISABLED_testNavigateToBadPage
#else
#define MAYBE_testNavigateToBadPage testNavigateToBadPage
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithFastTimeout,
                       MAYBE_testNavigateToBadPage) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
#if defined(SLOW_BINARY)
  GTEST_SKIP() << "skip timeout test for slow binary";
#else
  // Client loads, and navigates to a new URL. We try to load the client again,
  // but it fails.
  WebUIStateListener listener(GetHost());
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  listener.WaitForWebUiState(mojom::WebUiState::kReady);
  ExecuteJsTest({.params = base::Value(0)});
  listener.WaitForWebUiState(mojom::WebUiState::kBeginLoad);
  listener.WaitForWebUiState(mojom::WebUiState::kError);

  // Open the glic window to trigger reloading the client.
  // This time the client should load, falling back to the original URL.
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value(1)});
#endif
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testCallingApiWhileHiddenRecordsMetrics) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();
  RunTestSequence(CloseGlic());

  base::HistogramTester histogram_tester;
  ContinueJsTest();
  histogram_tester.ExpectBucketCount("Glic.Api.RequestCounts.CreateTab",
                                     GlicRequestEvent::kRequestReceived, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.Api.RequestCounts.CreateTab",
      GlicRequestEvent::kRequestReceivedWhileHidden, 1);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testPinTabs) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsWhileClosing) {
  NavigateTabAndOpenGlicFloating();
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsWithTwoTabs) {
  // TODO(b/452687492): This crashes with multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  NavigateTabAndOpenGlicFloating();
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));
  ExecuteJsTest();
  browser()->tab_strip_model()->SelectPreviousTab();
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testPinTabsFailsWhenDoesnotExist) {
  // Pinning a non existing tab id should fail.
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testPinTabsStatePersistWhenClosePanelAndReopen) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  NavigateTabAndOpenGlicFloating();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});

  RunTestSequence(OpenGlicFloatingWindow());
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsStatePersistWhenClientRestarts) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  NavigateTabAndOpenGlicFloating();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest(
      {.params = base::Value(base::Value::Dict()
                                 .Set("tabId", base::NumberToString(tab_id))
                                 .Set("isFirstRun", true))});

  WebUIStateListener listener(GetHost());
  ReloadGlicWebui();
  listener.WaitForWebUiState(mojom::WebUiState::kUninitialized);

  ExecuteJsTest(
      {.params = base::Value(base::Value::Dict().Set("isFirstRun", false))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testPinTabsFailsWhenIncognitoWindow) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);
  NavigateTabAndOpenGlicFloating();

  // Open a new incognito window.
  auto* incognito = CreateIncognitoBrowser();
  const GURL page_url = InProcessBrowserTest::embedded_test_server()->GetURL(
      "/glic/browser_tests/test.html");
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, page_url, std::nullopt, incognito));
  const int incognito_tab_id =
      GetTabId(incognito->tab_strip_model()->GetActiveWebContents());

  ExecuteJsTest(
      {.params = base::Value(base::Value::Dict().Set(
           "incognitoTabId", base::NumberToString(incognito_tab_id)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsFailsWhenNotPinned) {
  // TODO(bryantchandler): This segfauts on multi-instance. Fix and re-enable.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  NavigateTabAndOpenGlicFloating();
  // Unpinning a tab that is not pinned should fail.
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinAllTabs) {
  // TODO(bryantchandler): This has a UAF on multi-instance. Fix and re-enable.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  NavigateTabAndOpenGlicFloating();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testPinTabsHaveNoEffectOnFocusedTab) {
  // In multi-instance, pinned tabs do have an effect on the focused tab.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testUnpinTabsThatNavigateInBackground) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Use HTTPS test server for this test to test same-origin navigation.
  ASSERT_TRUE(embedded_https_test_server().Start());

  RunTestSequence(
      InstrumentTab(kFirstTab),
      NavigateWebContents(kFirstTab, embedded_https_test_server().GetURL(
                                         "a.com", "/test_data/page.html?one")),

      AddInstrumentedTab(kSecondTab, embedded_https_test_server().GetURL(
                                         "a.com", "/test_data/page.html?two")));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest();

  RunTestSequence(
      // Navigate to a different origin. Because it's hidden and the glic window
      // is hidden, it will be unpinned.
      NavigateWebContents(kSecondTab,
                          embedded_https_test_server().GetURL(
                              "b.com", "/test_data/page.html?changedTwo")),
      // Navigate to the same origin, this tab should not be unpinned.
      NavigateWebContents(kFirstTab,
                          embedded_https_test_server().GetURL(
                              "a.com", "/test_data/page.html?sameOrigin")),
      // Show the glic window and navigate the remaining tab. It should not be
      // unpinned.
      ToggleGlicWindow(GlicWindowMode::kDetached),
      NavigateWebContents(kFirstTab,
                          embedded_https_test_server().GetURL(
                              "b.com", "/test_data/page.html?changedOne")));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest, testTabDataUpdateOnUrlChangeForPinnedTab) {
  NavigateTabAndOpenGlicFloating();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Navigate to another page in the first tab.
  GURL new_url = embedded_test_server()->GetURL(
      "/glic/browser_tests/test.html?changed=true");
  RunTestSequence(NavigateWebContents(kFirstTab, new_url));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTest,
                       testTabDataUpdateOnFaviconChangeForPinnedTab) {
  NavigateTabAndOpenGlicFloating();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  const int tab_id = GetTabId(web_contents);
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Add favicon to the webcontents.
  const char* script =
      "var link = document.createElement('link');"
      "link.rel = 'icon';"
      "link.href= '../../../glic/youtube_favicon_16x16.png';"
      "document.head.appendChild(link);";
  ASSERT_TRUE(content::ExecJs(web_contents, script));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromTabIgnorePermissionWhenPinned) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // Try to extract context from an arbitrary tab without permission, while it's
  // unpinned and then pinned.
  ExecuteJsTest();

  // Should have one error logged for tab context permission not granted.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromTab.Error"),
      UnorderedElementsAre(Pair(
          "Glic.Api.GetContextFromTab.Error.Text",
          BucketsAre(Bucket(GlicGetContextFromTabError::
                                kPermissionDeniedContextPermissionNotEnabled,
                            1)))));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromTabFailDifferentlyBasedOnPermission) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  // For unfocused unpinned tabs, getTabContext call fail with different error
  // messages based on context sharing permission state.
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Two different permission errors should have been reported.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromTab.Error"),
      UnorderedElementsAre(
          Pair("Glic.Api.GetContextFromTab.Error.Text",
               BucketsAre(
                   Bucket(GlicGetContextFromTabError::
                              kPermissionDeniedContextPermissionNotEnabled,
                          1),
                   Bucket(GlicGetContextFromTabError::kPermissionDenied, 1)))));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromTabFailsIfNotPinned) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  const int tab_id =
      GetTabId(browser()->tab_strip_model()->GetActiveWebContents());
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest({.params = base::Value(base::Value::Dict().Set(
                     "tabId", base::NumberToString(tab_id)))});

  // Should have one error logged for tab context permission not granted.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromTab.Error"),
      UnorderedElementsAre(Pair(
          "Glic.Api.GetContextFromTab.Error.Text",
          BucketsAre(Bucket(GlicGetContextFromTabError::
                                kPermissionDeniedContextPermissionNotEnabled,
                            1)))));
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetContextFromTabFailsIfDoesNotExist) {
  ExecuteJsTest();

  // TODO(b/450026474): Multi-instance fails the metrics check because the
  // starting web client mode is not set.
  if (GetParam().multi_instance) {
    return;
  }
  // Should have one error logged for tab context permission not granted.
  EXPECT_THAT(
      histogram_tester->GetAllSamplesForPrefix(
          "Glic.Api.GetContextFromTab.Error"),
      UnorderedElementsAre(Pair(
          "Glic.Api.GetContextFromTab.Error.Text",
          BucketsAre(Bucket(GlicGetContextFromTabError::kTabNotFound, 1)))));
}

// TODO(crbug.com/441588906): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(GlicApiTestWithOneTab,
                       DISABLED_testFetchInactiveTabScreenshot) {
  // Untested on multi-instance.
  SKIP_TEST_FOR_MULTI_INSTANCE();

  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));

  ExecuteJsTest();

  browser()->tab_strip_model()->SelectPreviousTab();

  ContinueJsTest();
}

// Win-asan is flaky.
#if (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  DISABLED_testFetchInactiveTabScreenshotWhileMinimized
#else
#define MAYBE_testFetchInactiveTabScreenshotWhileMinimized \
  testFetchInactiveTabScreenshotWhileMinimized
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testFetchInactiveTabScreenshotWhileMinimized) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  RunTestSequence(AddInstrumentedTab(kSecondTab, page_url()));
  bool can_fetch_screenshot = BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC);

  ExecuteJsTest({.params = base::Value(can_fetch_screenshot)});

  browser()->tab_strip_model()->SelectPreviousTab();
  browser()->window()->Minimize();

  ContinueJsTest();
}

class GlicApiTestUserStatusCheckTest : public GlicApiTestWithOneTab {
 protected:
  void SetUpOnMainThread() override {
    GlicApiTestWithOneTab::SetUpOnMainThread();
    GetService()->enabling().SetUserStatusFetchOverrideForTest(
        base::BindRepeating(&GlicApiTestUserStatusCheckTest::UserStatusFetch,
                            base::Unretained(this)));
  }

  void UserStatusFetch(
      base::OnceCallback<void(const CachedUserStatus&)> callback) {
    user_status_fetch_count_++;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), user_status_));
  }

  CachedUserStatus user_status_;
  unsigned int user_status_fetch_count_ = 0;
};

void UpdatePrimaryAccountToBeManaged(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  account_info.hosted_domain = gaia::ExtractDomainName(account_info.email);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestUserStatusCheckTest,
                       testMaybeRefreshUserStatus) {
  Profile* profile = browser()->profile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::DISABLED_BY_ADMIN;
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin();
  }));
  EXPECT_GE(user_status_fetch_count_, 1u);
}

IN_PROC_BROWSER_TEST_P(GlicApiTestUserStatusCheckTest,
                       testMaybeRefreshUserStatusThrottled) {
  // As previous, but requests several updates (e.g., as though many errors
  // were processed around the same time). An "enabled" status is assumed as
  // otherwise the client will be unloaded.
  //
  // These expectations are a little loose, because we can't use mock time in
  // browser tests yet, but they should be sufficient to catch a total lack of
  // throttling, at least.

  Profile* profile = browser()->profile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());
  user_status_.user_status_code = UserStatusCode::ENABLED;
  ExecuteJsTest();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return user_status_fetch_count_ >= 2;
  })) << "There should be at least two fetches (initial and delayed)";
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Seconds(5));
    loop.Run();
  }
  EXPECT_LT(user_status_fetch_count_, 5u)
      << "We should not send most of the fetches";
}

// Given the time-based nature of debouncing, testing with non-mocked clocks can
// be flaky. This suite increases the applied delays to reduce the the chance of
// flakiness. This suite is disabled on all slow binaries.
#if defined(SLOW_BINARY)
#define MAYBE_GlicApiTestWithOneTabMoreDebounceDelay \
  DISABLED_GlicApiTestWithOneTabMoreDebounceDelay
#else
#define MAYBE_GlicApiTestWithOneTabMoreDebounceDelay \
  GlicApiTestWithOneTabMoreDebounceDelay
#endif
class MAYBE_GlicApiTestWithOneTabMoreDebounceDelay
    : public GlicApiTestWithOneTab {
 public:
  MAYBE_GlicApiTestWithOneTabMoreDebounceDelay() {
    features2_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{
            features::kGlicTabFocusDataDedupDebounce,
            {
                // Set an arbitrarily high debounce delay to avoid flakiness.
                {features::kGlicTabFocusDataDebounceDelayMs.name, "1000"},
            },
        }},
        /*disabled_features=*/
        {});
  }

 private:
  base::test::ScopedFeatureList features2_;
};

// Confirm that the web client receives a minimal number of focused tab updates
// by triggering events that generate such updates.
// TODO(b/424242331): figure out why this is failing on linux-rel bot.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_testSingleFocusedTabUpdatesOnTabEvents \
  testSingleFocusedTabUpdatesOnTabEvents
#else
#define MAYBE_testSingleFocusedTabUpdatesOnTabEvents \
  DISABLED_testSingleFocusedTabUpdatesOnTabEvents
#endif
IN_PROC_BROWSER_TEST_P(MAYBE_GlicApiTestWithOneTabMoreDebounceDelay,
                       MAYBE_testSingleFocusedTabUpdatesOnTabEvents) {
  SKIP_TEST_FOR_MULTI_INSTANCE();
  // Initial state with first tab.
  ExecuteJsTest();

  // Navigate to another page in the first tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                     "/scrollable_page_with_content.html")));
  ContinueJsTest();

  // Open a new tab that becomes active and navigate to a another page.
  RunTestSequence(AddInstrumentedTab(
      kSecondTab, InProcessBrowserTest::embedded_test_server()->GetURL(
                      "/glic/browser_tests/test.html")));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPinCandidatesSingleTab) {
  // In multi-instance mode, the tab is automatically pinned. Unpin it now.
  if (GetParam().multi_instance) {
    GetGlicInstanceImpl()->sharing_manager().UnpinAllTabs();
  }
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetPinCandidatesWithPanelClosed) {
  ExecuteJsTest();
  RunTestSequence(AddInstrumentedTab(
      kSecondTab,
      embedded_test_server()->GetURL("/glic/browser_tests/test.html")));
  ContinueJsTest();
  // Opens the panel again.
  RunTestSequence(ToggleGlicWindow(GlicWindowMode::kDetached));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testSendsViewChangeRequestOnTaskIconOrGlicButtonToggle) {
  TODO_SKIP_BROKEN_MULTI_INSTANCE_TEST();
  CurrentViewListener listener(GetHost());
  listener.WaitForCurrentView(mojom::CurrentView::kConversation);
  StartTaskAndShowActorTaskIcon();
  ExecuteJsTest();
  RunTestSequence(ToggleGlicWindowFromSource(
      GlicWindowMode::kDetached, kGlicActorTaskIconElementId,
      mojom::InvocationSource::kActorTaskIcon));
  ContinueJsTest();
  listener.WaitForCurrentView(mojom::CurrentView::kActuation);
  RunTestSequence(ToggleGlicWindowFromSource(
      GlicWindowMode::kDetached, kGlicButtonElementId,
      mojom::InvocationSource::kTopChromeButton));
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testSwitchConversationToExistingInstance) {
  if (!GetParam().multi_instance) {
    GTEST_SKIP() << "Only supported in multi-instance mode.";
  }
  // Open glic. It will register a conversation.
  ExecuteJsTest({.params = base::Value("first")});

  // Open a second tab and second glic instance. It will switch conversations
  // resulting in deleting the second glic instance.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);
  TrackGlicInstanceWithTabIndex(1);
  RunTestSequence(InstrumentTab(kSecondTab),
                  OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents));
  ExecuteJsTest({.params = base::Value("second")});

  // This should continue the test in the first instance, because tab 2 is now
  // bound to that instance.
  ContinueJsTest();
}

class GlicGetHostCapabilityApiTest : public GlicApiTestWithOneTab {
 public:
  GlicGetHostCapabilityApiTest() {
    const bool enable_features = GetParam().enable_scroll_to_pdf;
    if (enable_features) {
      // TODO(b/444002499) - add features::kGlicMultiInstance when test support
      //  enabled.
      std::vector<base::test::FeatureRefAndParams> enabled_features = {
          {features::kGlicScrollTo, {{"glic-scroll-to-pdf", "true"}}},
          {features::kGlicPanelResetSizeAndLocationOnOpen, {}}};
      scoped_feature_list_.InitWithFeaturesAndParameters(
          enabled_features,
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{},
          /*disabled_features=*/{});
    }
  }
  ~GlicGetHostCapabilityApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicGetHostCapabilityApiTest, testGetHostCapabilities) {
  const bool enable_features = GetParam().enable_scroll_to_pdf;
  if (enable_features) {
#if BUILDFLAG(ENABLE_PDF)
    // The host is only capable of scrolling on PDF document if the feature flag
    // is enabled, and on PDF-enabled platforms.
    ExecuteJsTest({
        .params = base::Value(base::Value::List().Append(
            base::to_underlying(mojom::HostCapability::kScrollToPdf))),
    });
#else
    ExecuteJsTest();
#endif
  } else {
    ExecuteJsTest();
  }
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPageMetadata) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPageMetadataInvalidTabId) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPageMetadataEmptyNames) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       testGetPageMetadataMultipleSubscriptions) {
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPageMetadataUpdates) {
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused. We can now modify the page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Change the content of the 'author' meta tag from "George" to "Ruth".
  const char* script =
      "document.querySelector('meta[name=\"author\"]').setAttribute('content', "
      "'Ruth')";
  ASSERT_TRUE(content::ExecJs(web_contents, script));

  // Continue the JS test to verify the metadata update.
  ContinueJsTest();
}

// TODO(https://crbug.com/449764057): Flakes/fails on all by windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_testGetPageMetadataOnNavigation testGetPageMetadataOnNavigation
#else
#define MAYBE_testGetPageMetadataOnNavigation \
  DISABLED_testGetPageMetadataOnNavigation
#endif
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       MAYBE_testGetPageMetadataOnNavigation) {
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused. We can now navigate the tab.
  RunTestSequence(NavigateWebContents(
      kFirstTab,
      InProcessBrowserTest::embedded_test_server()->GetURL("/title1.html")));

  // Continue the JS test to verify the metadata update.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testGetPageMetadataTabDestroyed) {
  // TODO(harringtond): Re-enable this when multi-instance supports floating.
  // We can float the window before closing the tab.
  SKIP_TEST_FOR_MULTI_INSTANCE();
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused.
  content::WebContents* web_contents_to_close =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Add a new tab to keep the browser alive before closing the active tab.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          web_contents_to_close),
      CLOSE_NONE);

  // Continue the JS test to verify the observable is completed.
  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab, testAdditionalContext) {
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused. We can now send the additional context.
  auto context = mojom::AdditionalContext::New();
  std::vector<mojom::AdditionalContextPartPtr> parts;
  {
    auto context_data = mojom::ContextData::New();
    context_data->mime_type = "text/plain";
    context_data->data =
        mojo_base::BigBuffer(std::vector<uint8_t>{'t', 'e', 's', 't'});
    parts.push_back(
        mojom::AdditionalContextPart::NewData(std::move(context_data)));
  }
  {
    auto screenshot = mojom::Screenshot::New();
    screenshot->width_pixels = 10;
    screenshot->height_pixels = 20;
    screenshot->mime_type = "image/png";
    screenshot->data = std::vector<uint8_t>{1, 2, 3, 4};
    screenshot->origin_annotations = mojom::ImageOriginAnnotations::New();
    parts.push_back(
        mojom::AdditionalContextPart::NewScreenshot(std::move(screenshot)));
  }

  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kFirstTab);
  auto* web_contents = AsInstrumentedWebContents(element)->web_contents();
  context->name = "part with everything";
  context->tab_id = GetTabId(web_contents);
  context->origin = url::Origin::Create(web_contents->GetLastCommittedURL());
  context->frameUrl = web_contents->GetLastCommittedURL();

  {
    auto web_page_data = mojom::WebPageData::New();
    web_page_data->main_document = mojom::DocumentData::New();
    web_page_data->main_document->origin =
        url::Origin::Create(context->frameUrl.value());
    web_page_data->main_document->inner_text = "some inner text";
    web_page_data->main_document->inner_text_truncated = false;
    parts.push_back(
        mojom::AdditionalContextPart::NewWebPageData(std::move(web_page_data)));
  }

  {
    parts.push_back(mojom::AdditionalContextPart::NewAnnotatedPageData(
        mojom::AnnotatedPageData::New()));
  }

  {
    auto pdf_data = mojom::PdfDocumentData::New();
    pdf_data->origin = url::Origin::Create(context->frameUrl.value());
    pdf_data->size_limit_exceeded = false;
    pdf_data->pdf_data = std::vector<uint8_t>{'p', 'd', 'f'};
    parts.push_back(
        mojom::AdditionalContextPart::NewPdfDocumentData(std::move(pdf_data)));
  }
  context->parts = std::move(parts);

  GetService()->SendAdditionalContext(tabs::TabHandle(GetTabId(web_contents)),
                                      std::move(context));

  // Continue the JS test to verify the additional context is received.
  ContinueJsTest();
}

// TODO(gklassen): Re-enable this test once I figure out how to doscard the tab
// while preserving the test harness.
IN_PROC_BROWSER_TEST_P(GlicApiTestWithOneTab,
                       DISABLED_testGetPageMetadataWebContentsChanged) {
  // Runs the JS test until the first `advanceToNextStep()`.
  ExecuteJsTest();

  // The JS test is now paused.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Discard the tab. This will destroy the WebContents.
  resource_coordinator::TabLifecycleUnitExternal::FromWebContents(web_contents)
      ->DiscardTab(::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  // Wait for the tab to be discarded.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return web_contents->WasDiscarded(); }));

  // Select the tab to reload it. This will create a new WebContents.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->active_index());
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(new_web_contents));

  // Change the content of the 'author' meta tag from "George" to "Ruth".
  const char* script =
      "document.querySelector('meta[name=\"author\"]').setAttribute('content', "
      "'Ruth')";
  ASSERT_TRUE(content::ExecJs(new_web_contents, script));

  // Continue the JS test to verify the metadata update.
  ContinueJsTest();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicGetHostCapabilityApiTest,
    testing::Values(TestParams{}, TestParams{.enable_scroll_to_pdf = true}),
    &WithTestParams::PrintTestVariant);

auto DefaultTestParamSet() {
  return testing::Values(TestParams{.multi_instance = false},
                         TestParams{.multi_instance = true});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicApiTestWithOneTab,
#if defined(SLOW_BINARY)
    // Even the test setup sometimes doesn't finish on ASAN for multi-instance.
    testing::Values(TestParams{.multi_instance = false}),
#else
    DefaultTestParamSet(),
#endif
    &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithDefaultTabContextEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithDefaultTabContextDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithMqlsIdGetterEnabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithMqlsIdGetterDisabled,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(
    ,
    GlicApiTestWithOneTabAndPreloading,
    // TODO(harringtond): Test setup fails w/ multi instance.
    testing::Values(TestParams{.multi_instance = false}),
    &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithOneTabAndContextualCueing,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestWithFastTimeout,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestRuntimeFeatureOff,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestSystemSettingsTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         GlicApiTestUserStatusCheckTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);
INSTANTIATE_TEST_SUITE_P(,
                         MAYBE_GlicApiTestWithOneTabMoreDebounceDelay,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

}  // namespace
}  // namespace glic
