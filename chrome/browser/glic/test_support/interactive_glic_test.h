// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_

#include <map>
#include <sstream>
#include <string_view>

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace glic {
class GlicWindowControllerImpl;
}

namespace glic::test {

extern const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton;
extern const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel;

// Mixin class that adds a mock glic to the current browser.
// If all you need is the combination of this + interactive browser test, use
// `InteractiveGlicTest` (defined below) instead.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest> &&
           std::derived_from<T, InteractiveBrowserTestApi>)
class InteractiveGlicTestMixin : public T {
 public:
  // Determines whether this is an attached or detached Glic window.
  // WARNING: This is no longer very meaningful, and should be replaced. These
  // do not provide the ability to open glic as a floating window when in
  // multi-instance mode. See the comments just below.
  enum GlicWindowMode {
    // Opens glic by pressing the Glic button on the browser.
    // In multi-instance, this means it will open glic as a side panel.
    // Otherwise, glic is opened as a floating window.
    kAttached,
    // Opens glic by calling ShowDetachedForTesting() on the window controller.
    // There may not be a good reason for using this.
    kDetached,
  };

  // What portions of the glic window should be instrumented on open.
  enum GlicInstrumentMode {
    // Instruments the host as `kGlicHostElementId` and contents as
    // `kGlicContentsElementId`.
    kHostAndContents,
    // Instruments only the host as `kGlicHostElementId`.
    kHostOnly,
    // Does not instrument either.
    kNone
  };

  // Constructor that takes `FieldTrialParams` and a
  // `GlicTestEnvironmentConfig`, then forwards the rest of the args.
  template <typename... Args>
  explicit InteractiveGlicTestMixin(
      const base::FieldTrialParams& glic_params,
      const GlicTestEnvironmentConfig& glic_config,
      Args&&... args)
      : T(std::forward<Args>(args)...), glic_test_environment_(glic_config) {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlic, glic_params},
         {features::kTabstripComboButton, {}},
         {features::kGlicRollout, {}},
         {features::kGlicKeyboardShortcutNewBadge, {}}},
        {});
  }

  // Default constructor (no forwarded args or field trial parameters).
  InteractiveGlicTestMixin()
      : InteractiveGlicTestMixin(base::FieldTrialParams(),
                                 GlicTestEnvironmentConfig()) {}

  explicit InteractiveGlicTestMixin(const base::FieldTrialParams& glic_params)
      : InteractiveGlicTestMixin(glic_params, GlicTestEnvironmentConfig()) {}

  // Constructor with no field trial params; all arguments are forwarded to the
  // base class.
  template <typename Arg, typename... Args>
    requires(!std::same_as<base::FieldTrialParams, std::remove_cvref_t<Arg>>)
  explicit InteractiveGlicTestMixin(Arg&& arg, Args&&... args)
      : InteractiveGlicTestMixin(base::FieldTrialParams(),
                                 std::forward<Arg>(arg),
                                 std::forward<Args>(args)...) {}

  ~InteractiveGlicTestMixin() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    T::SetUpBrowserContextKeyedServices(context);
  }

  void SetUpOnMainThread() override {
    LOG(INFO) << "InteractiveGlicTest: setting up base fixture";
    T::SetUpOnMainThread();
    LOG(INFO) << "InteractiveGlicTest: setting up";

    Test::embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    Test::embedded_https_test_server().ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    Test::embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/webui/glic/");
    Test::embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/test/data/webui/glic/");

    ASSERT_TRUE(test_server_handle_ =
                    Test::embedded_test_server()->StartAndReturnHandle());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    std::ostringstream path;
    path << glic_page_path_;

    // Append the query parameters to the URL.
    bool first_param = true;
    auto encode = [](const std::string_view& value) {
      url::RawCanonOutputT<char> encoded;
      url::EncodeURIComponent(value, &encoded);
      return std::string(encoded.view());
    };
    for (const auto& [key, value] : mock_glic_query_params_) {
      path << (first_param ? "?" : "&");
      first_param = false;
      path << encode(key);
      if (!value.empty()) {
        path << "=" << encode(value);
      }
    }

    auto* command_line = base::CommandLine::ForCurrentProcess();
    guest_url_ = Test::embedded_test_server()->GetURL(path.str());
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL,
                                    guest_url_.spec());
    GURL fre_url = glic_fre_url_.value_or(
        Test::embedded_test_server()->GetURL("/glic/test_client/fre.html"));
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url.spec());
    LOG(INFO) << "InteractiveGlicTest: done setting up";
  }

  void TearDownOnMainThread() override { T::TearDownOnMainThread(); }

  void SetGlicPagePath(const std::string& glic_page_path) {
    glic_page_path_ = glic_page_path;
  }

  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return WaitForAndInstrumentGlicMultiInstance(instrument_mode);
    }
    return WaitForAndInstrumentGlic(instrument_mode, window_controller());
  }

  auto WaitForAndInstrumentGlicMultiInstance(
      GlicInstrumentMode instrument_mode) {
    Api::MultiStep steps;
    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::InAnyContext(
                Api::Steps(Api::InstrumentNonTabWebView(kGlicHostElementId,
                                                        kGlicViewElementId),
                           Api::InstrumentInnerWebContents(
                               kGlicContentsElementId, kGlicHostElementId, 0),
                           Api::Log("Waiting for Glic web contents ready"),
                           Api::WaitForWebContentsReady(kGlicContentsElementId),
                           Api::Log("Glic web contents is ready"))),
            Api::PollUntil(
                [&]() -> bool {
                  GlicInstance* instance = GetGlicInstanceImpl();
                  if (!instance) {
                    LOG(ERROR)
                        << "No glic instance for " << DescribeGlicTracking();
                    return false;
                  }
                  if (!instance->IsShowing()) {
                    LOG(ERROR) << "Glic not showing";
                    return false;
                  }
                  if (!instance->host().IsReady()) {
                    LOG(ERROR) << "Glic host not ready";
                    return false;
                  }
                  return true;
                },
                "Glic not ready"));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
      default:
        NOTREACHED();
    }
    return steps;
  }

  // Ensures that the WebContents for some combination of glic host and contents
  // are instrumented, per `instrument_mode`. Takes a window controller, to
  // permit instrumenting for a different profile.
  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode,
                                GlicWindowController& window_controller) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;

    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller)),
            Api::InAnyContext(Api::Steps(
                Api::InstrumentNonTabWebView(kGlicHostElementId,
                                             kGlicViewElementId),
                Api::InstrumentInnerWebContents(kGlicContentsElementId,
                                                kGlicHostElementId, 0),
                Api::WaitForWebContentsReady(kGlicContentsElementId))),
            Api::WaitForState(internal::kGlicWindowControllerState,
                              GlicWindowController::State::kOpen),
            Api::StopObservingState(internal::kGlicWindowControllerState)
            /*, WaitForElementVisible(kPathToGuestPanel)*/);
        break;
      case GlicInstrumentMode::kHostOnly:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller)),
            Api::InAnyContext(Api::InstrumentNonTabWebView(kGlicHostElementId,
                                                           kGlicViewElementId)),
            Api::WaitForState(
                internal::kGlicWindowControllerState,
                testing::Matcher<GlicWindowController::State>(testing::AnyOf(
                    GlicWindowController::State::kWaitingForGlicToLoad,
                    GlicWindowController::State::kOpen))),
            Api::StopObservingState(internal::kGlicWindowControllerState));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
    }

    Api::AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
    return steps;
  }
  // Activate one of the glic entrypoints.
  // If `instrument_glic_contents` is true both the host and contents will be
  // instrumented (see `WaitForAndInstrumentGlic()`) else only the host will be
  // instrumented (`WaitForAndInstrumentGlicHostOnly()`).
  auto OpenGlicWindow(GlicWindowMode window_mode,
                      GlicInstrumentMode instrument_mode =
                          GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps =
        Api::Steps(Api::Log("Opening glic window"), CheckGlicIsClosed(),
                   // Technically, this toggles the window, but we've
                   // already ensured that it's closed.
                   ToggleGlicWindow(window_mode),
                   WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  auto OpenGlicFloatingWindow(GlicInstrumentMode instrument_mode =
                                  GlicInstrumentMode::kHostAndContents) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      auto steps = Api::Steps(Api::Do([&]() {
                                GetInstanceCoordinator().Toggle(
                                    /*browser=*/nullptr, true,
                                    mojom::InvocationSource::kOsButton);
                              }),
                              WaitForAndInstrumentGlic(instrument_mode));
      Api::AddDescriptionPrefix(steps, "OpenGlicFloatingWindow");
      return steps;
    } else {
      return OpenGlicWindow(GlicWindowMode::kDetached, instrument_mode);
    }
  }

  // Toggles Glic through one of the entrypoints.
  // Does not wait for Glic to open or close, tests using this should check for
  // the correct window state after toggling.
  auto ToggleGlicWindow(GlicWindowMode window_mode) {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::PressButton(kGlicButtonElementId)
          .SetContext(BrowserElements::From(browser())->GetContext());
    }
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(kGlicButtonElementId)
            .SetContext(BrowserElements::From(browser())->GetContext());
      case GlicWindowMode::kDetached:
        return Api::Do(
            [this] { window_controller().ShowDetachedForTesting(); });
    }
  }

  // Toggles Glic through a specific InvocationSource.
  auto ToggleGlicWindowFromSource(GlicWindowMode window_mode,
                                  ui::ElementIdentifier element_id,
                                  mojom::InvocationSource invocation_source) {
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(element_id);
      case GlicWindowMode::kDetached:
        return Api::Do([this, invocation_source] {
          window_controller().Toggle(browser(), false, invocation_source);
        });
    }
  }

  // Close the glic panel, regardless of the current state. Unlike
  // `CloseGlicWindow()`, this will close the window even if the glic client is
  // not connected, and will do nothing if the window is already closed.
  auto CloseGlic() {
    return Api::Do([&]() {
      if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
        GlicUiEmbedder* embedder = GetGlicUiEmbedder();
        if (embedder) {
          embedder->Close();
        }
      } else {
        window_controller().Close();
      }
    });
  }

  auto ClickWebuiCloseButton() {
    auto steps = Api::Steps(
        Api::WaitForElementVisible(kGlicHostElementId, {"body"}),
        Api::ExecuteJsAt(
            kGlicHostElementId, {".close-button"}, "(el)=>el.click()",
            InteractiveBrowserTestApi::ExecuteJsMode::kWaitForCompletion));
  }

  // Ensures a mock glic button is present and then clicks it. Works even if the
  // element is off-screen.
  auto ClickMockGlicElement(
      const WebContentsInteractionTestUtil::DeepQuery& where,
      const bool click_closes_window = false) {
    auto steps = Api::Steps(
        // Note: Elements on the test client don't need to be in the viewport to
        // be used. Ideally we would wait until the element is visible, but not
        // necessarily on screen. Because we don't have any elements that get
        // hidden on the test client, waiting for body visibility is good
        // enough.
        Api::WaitForElementVisible(kGlicContentsElementId, {"body"}),
        // TODO(dfried): Figure out why Api::CheckJsResultAt() here doesn't
        // work. Error:
        // Interactive test failed on step 28 (ClickMockGlicElement:
        // CheckJsResultAt( {"#contextAccessIndicator"}, " ... with reason
        // kSequenceDestroyed; step type kShown; id ElementIdentifier
        // kGlicContentsElementId.
        Api::ExecuteJsAt(
            kGlicContentsElementId, where, "(el)=>el.click()",
            click_closes_window
                ? InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget
                : InteractiveBrowserTestApi::ExecuteJsMode::
                      kWaitForCompletion));

    Api::AddDescriptionPrefix(steps, "ClickMockGlicElement");
    return steps;
  }

  // Closes the glic window, which must be open.
  //
  // TODO: this only works if glic is actually loaded; handle the case where the
  // contents pane has either not loaded or failed to load.
  auto CloseGlicWindow() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps =
        Api::InAnyContext(Api::Steps(CheckGlicWindowIsOpen(), CloseGlic(),
                                     Api::WaitForHide(kGlicViewElementId)));
    Api::AddDescriptionPrefix(steps, "CloseGlicWindow");
    return steps;
  }

  auto SimulateAcceleratorPress(const ui::Accelerator& accelerator) {
    return Api::Do([this, accelerator] {
      CHECK(GetGlicWidget());
      gfx::NativeWindow target_window = GetGlicWidget()->GetNativeWindow();
#if (USE_AURA)
      ui::test::EventGenerator event_generator(target_window->GetRootWindow(),
                                               target_window);
#else
      ui::test::EventGenerator event_generator(target_window);
#endif
      event_generator.set_target(ui::test::EventGenerator::Target::WINDOW);
      event_generator.PressAndReleaseKeyAndModifierKeys(
          accelerator.key_code(), accelerator.modifiers());
    });
  }

  auto CheckControllerHasWidget(bool expect_widget) {
    return Api::CheckResult([this]() { return GetGlicWidget() != nullptr; },
                            expect_widget, "CheckControllerHasWidget");
  }

  auto CheckControllerShowing(bool expect_showing) {
    return Api::CheckResult(
        [this]() {
          if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
            return GetGlicUiEmbedder() && GetGlicUiEmbedder()->IsShowing();
          } else {
            return GetWindowControllerImpl().IsShowing();
          }
        },
        expect_showing, "CheckControllerShowing");
  }

  auto CheckControllerWidgetMode(GlicWindowMode mode) {
    return Api::CheckResult(
        [this]() {
          if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
            if (!GetGlicInstance()) {
              return GlicWindowMode::kAttached;
            }
            return GetGlicInstance()->IsAttached() ? GlicWindowMode::kAttached
                                                   : GlicWindowMode::kDetached;
          } else {
            return GetWindowControllerImpl().IsAttached()
                       ? GlicWindowMode::kAttached
                       : GlicWindowMode::kDetached;
          }
        },
        mode, "CheckControllerWidgetMode");
  }

  auto CheckPointIsWithinDraggableArea(const gfx::Point& point,
                                       bool expect_within_area) {
    return Api::CheckResult(
        [this, point]() {
          return GetWindowControllerImpl()
              .GetGlicViewForTesting()
              ->IsPointWithinDraggableArea(point);
        },
        expect_within_area,
        "CheckPointIsWithinDraggableArea_" + point.ToString());
  }

  auto CheckIfAttachedToBrowser(Browser* new_browser) {
    return Api::CheckResult(
        [this] { return window_controller().attached_browser(); }, new_browser,
        "attached to the other browser");
  }

  auto CheckWidgetMinimumSize(const gfx::Size& size) {
    // Size can't be smaller than the initial size.
    auto expected_size = glic::GlicWidget::GetInitialSize();
    expected_size.SetToMax(size);
    return Api::CheckResult(
        [this]() { return GetGlicWidget()->GetMinimumSize(); }, expected_size,
        "CheckWidgetMinimumSize");
  }

  auto CheckTabCount(int expected_count) {
    return Api::CheckResult(
        [this] { return browser()->tab_strip_model()->GetTabCount(); },
        expected_count, "CheckTabCount");
  }

  auto CheckOcclusionTracked(bool expect_is_tracked) {
    return Api::CheckResult(
        [this]() {
          return base::Contains(PictureInPictureWindowManager::GetInstance()
                                    ->GetOcclusionTracker()
                                    ->GetPictureInPictureWidgetsForTesting(),
                                GetGlicWidget());
        },
        expect_is_tracked, "CheckOcclusionTracked");
  }

  auto Wait(base::TimeDelta timeout) {
    auto observer = std::make_unique<internal::WaitingStateObserver>();
    auto observer_ptr = observer.get();
    return Api::Steps(
        Api::Do(base::BindRepeating(
            [](internal::WaitingStateObserver* observer,
               base::TimeDelta timeout) { observer->Start(timeout); },
            base::Unretained(observer_ptr), timeout)),
        Api::ObserveState(glic::test::internal::kDelayState,
                          std::move(observer)),
        Api::WaitForState(glic::test::internal::kDelayState, true));
  }

  auto WaitForCanResizeEnabled(bool enabled) {
    return Api::Steps(
        Api::ObserveState(internal::kGlicWindowControllerResizeState,
                          std::ref(window_controller())),
        Api::Log("WaitForCanResize: ", enabled ? "true" : "false"),
        Api::WaitForState(internal::kGlicWindowControllerResizeState, enabled),
        Api::StopObservingState(internal::kGlicWindowControllerResizeState));
  }

  content::RenderFrameHost* FindGlicGuestMainFrame() {
    Host* host = GetHost();
    if (!host) {
      return nullptr;
    }
    for (GlicPageHandler* handler : GetHost()->GetPageHandlersForTesting()) {
      if (handler->GetGuestMainFrame()) {
        return handler->GetGuestMainFrame();
      }
    }
    return nullptr;
  }

  content::WebContents* FindGlicWebUIContents() {
    Host* host = GetHost();
    return host ? host->webui_contents() : nullptr;
  }

  glic::GlicTestEnvironment& glic_test_environment() {
    return glic_test_environment_;
  }

  glic::GlicTestEnvironmentService& glic_test_service() {
    return *glic_test_environment_.GetService(browser()->GetProfile());
  }

  // Send a task state update to show the actor task icon in the tab strip.
  void StartTaskAndShowActorTaskIcon() {
    auto actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
    actor::TaskId task_id = actor_service->CreateTask();
    actor::ui::StartTask start_task_event(task_id);
    actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);
  }

  void ReloadGlicWebui() {
    Host* host = GetHost();
    CHECK(host);
    host->Reload();
  }

  void DisableWarming() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GetInstanceCoordinator().SetWarmingEnabledForTesting(false);
    } else {
      // Not supported for single-instance, as warming is disabled by feature
      // flag.
    }
  }

  // Same as `Api::AddInstrumentedTabWithOpener()`, but sets the `opener` to
  // the current glic instance web contents. This is useful to bind the glic
  // instance from the active tab to the newly created tab.
  InteractiveBrowserTestApi::MultiStep AddInstrumentedTabWithOpener(
      ui::ElementIdentifier id,
      GURL url,
      std::optional<int> at_index = std::nullopt) {
    auto steps = Api::Steps(
        Api::InstrumentNextTab(id),
        Api::WithElement(
            ui::test::internal::kInteractiveTestPivotElementId,
            base::BindLambdaForTesting([this, url,
                                        at_index](ui::TrackedElement* el) {
              Browser* const browser_ptr = browser();
              CHECK(browser_ptr) << "No browser";
              CHECK(GetHost());
              NavigateParams navigate_params(
                  browser_ptr, url, ui::PageTransition::PAGE_TRANSITION_TYPED);
              navigate_params.tabstrip_index = at_index.value_or(-1);
              navigate_params.disposition =
                  WindowOpenDisposition::NEW_FOREGROUND_TAB;
              navigate_params.opener =
                  GetHost()->webui_contents()->GetPrimaryMainFrame();
              CHECK(Navigate(&navigate_params));
            })),
        Api::WaitForWebContentsReady(id));
    Api::AddDescriptionPrefix(
        steps, base::StringPrintf("AddInstrumentedTabWithOpener( %s, %s, %d, )",
                                  id.GetName().c_str(), url.spec().c_str(),
                                  at_index.value_or(-1)));
    return steps;
  }

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  GlicWindowControllerImpl& GetWindowControllerImpl() {
    CHECK(!base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicWindowControllerImpl&>(
        glic_service()->window_controller());
  }

  GlicInstanceCoordinatorImpl& GetInstanceCoordinator() {
    CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicInstanceCoordinatorImpl&>(
        glic_service()->window_controller());
  }

  GlicInstanceImpl* GetGlicInstanceImpl() {
    CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
    return static_cast<GlicInstanceImpl*>(GetGlicInstance());
  }

  GlicUiEmbedder* GetGlicUiEmbedder() {
    GlicInstanceImpl* instance = GetGlicInstanceImpl();
    if (!instance) {
      return nullptr;
    }
    return instance->GetEmbedderForTab(browser()->GetActiveTabInterface());
  }

  views::View* GetGlicView() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GlicUiEmbedder* embedder = GetGlicUiEmbedder();
      if (!embedder) {
        return nullptr;
      }
      return embedder->GetView().get();
    }
    return GetWindowControllerImpl().GetGlicViewForTesting();
  }

  views::Widget* GetGlicWidget() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GlicUiEmbedder* embedder = GetGlicUiEmbedder();
      if (!embedder) {
        return nullptr;
      }
      auto* view = embedder->GetView().get();
      if (!view) {
        return nullptr;
      }
      return view->GetWidget();
    }
    return window_controller().GetGlicWidget();
  }

  Host* GetHost() {
    GlicInstance* instance = GetGlicInstance();
    if (!instance) {
      return nullptr;
    }
    return &instance->host();
  }

  auto CheckGlicWindowIsOpen() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::CheckResult(
          [this]() {
            views::View* view = GetGlicView();
            return view && view->GetVisible();
          },
          "glic panel must be open");
    }
    return EnsureGlicWindowState("glic window must be open",
                                 GlicWindowController::State::kOpen);
  }
  auto CheckGlicIsClosed() {
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      return Api::CheckResult(
          [this]() {
            views::View* view = GetGlicView();
            return !view || !view->GetVisible();
          },
          "glic panel must be closed");
    }
    return EnsureGlicWindowState("glic window must be closed",
                                 GlicWindowController::State::kClosed);
  }

  template <typename... M>
  auto EnsureGlicWindowState(const std::string& desc, M&&... matchers) {
    return Api::CheckResult([this]() { return window_controller().state(); },
                            testing::Matcher<GlicWindowController::State>(
                                testing::AnyOf(std::forward<M>(matchers)...)),
                            desc);
  }

  // Adds a query param to the URL that will be used to load the mock glic.
  // Must be called before `SetUpOnMainThread()`. Both `key` and `value` (if
  // specified) will be URL-encoded for safety.
  void add_mock_glic_query_param(const std::string_view& key,
                                 const std::string_view& value = "") {
    mock_glic_query_params_.emplace(key, value);
  }

  GURL GetGuestURL() {
    CHECK(guest_url_.is_valid()) << "Guest URL not yet configured.";
    return guest_url_;
  }

  void SetGlicFreUrlOverride(const GURL& url) { glic_fre_url_ = url; }

  // `InteractiveGlicTestMixin` is configured to operate a single browser, but
  // it can change which browser it operates. This changes the browser to be
  // used in functions of `InteractiveGlicTestMixin`.
  void SetActiveBrowser(Browser* browser) {
    active_browser_ = browser->AsWeakPtr();
  }

  // Returns the active browser.
  Browser* browser() {
    if (active_browser_) {
      return active_browser_.get();
    } else {
      CHECK(!active_browser_.WasInvalidated())
          << "SetActiveBrowser() was called, but that browser no longer "
             "exists.";
      return InProcessBrowserTest::browser();
    }
  }

  // Glic tracking functions. By default, this fixture applies operations toward
  // the glic instance in tab 0. You can change this behavior by calling one of
  // these functions.

  // Have all glic instance operations linked to a glic instance with this ID.
  void TrackGlicInstanceWithId(InstanceId id) {
    ClearGlicTracking();
    tracked_instance_id_ = id;
  }

  // Track the glic instance at a specific tab index.
  void TrackGlicInstanceWithTabIndex(int index) {
    ClearGlicTracking();
    glic_instance_tab_index_ = index;
  }

  // Track the glic instance at this tab.
  void TrackGlicInstanceWithTabHandle(tabs::TabInterface::Handle handle) {
    ClearGlicTracking();
    glic_instance_tab_handle_ = handle;
  }

  void TrackFloatingGlicInstance() {
    ClearGlicTracking();
    track_floating_glic_instance_ = true;
  }

  // Returns the currently tracked glic instance.
  GlicInstance* GetGlicInstance() {
    if (tracked_instance_id_) {
      for (GlicInstance* instance : window_controller().GetInstances()) {
        if (instance->id() == *tracked_instance_id_) {
          return instance;
        }
      }
      return nullptr;
    }

    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      if (track_floating_glic_instance_) {
        return GetInstanceCoordinator().GetInstanceWithFloaty();
      }
      if (glic_instance_tab_handle_) {
        if (glic_instance_tab_handle_->Get()) {
          return glic_service()->GetInstanceForTab(
              glic_instance_tab_handle_->Get());
        }
        return nullptr;
      }
      if (glic_instance_tab_index_ != std::nullopt) {
        return glic_service()->GetInstanceForTab(
            browser()->GetTabStripModel()->GetTabAtIndex(
                *glic_instance_tab_index_));
      }
      return glic_service()->GetInstanceForTab(
          browser()->GetTabStripModel()->GetTabAtIndex(0));
    }
    return glic_service()->GetInstanceForActiveTab(browser());
  }

 private:
  std::string DescribeGlicTracking() {
    if (tracked_instance_id_) {
      return base::StrCat({"Tracking glic instance with id ",
                           tracked_instance_id_->AsLowercaseString()});
    } else if (glic_instance_tab_index_) {
      return base::StrCat({"Tracking glic instance at tab index ",
                           base::NumberToString(*glic_instance_tab_index_)});

    } else if (glic_instance_tab_handle_) {
      if (!glic_instance_tab_handle_->Get()) {
        return "Tracking glic instance with INVALID tab handle";
      }
      return "Tracking glic instance with tab handle";
    } else if (track_floating_glic_instance_) {
      return "Tracking floating glic instance";
    }
    NOTREACHED();
  }

  void ClearGlicTracking() {
    tracked_instance_id_ = std::nullopt;
    glic_instance_tab_index_ = std::nullopt;
    glic_instance_tab_handle_ = std::nullopt;
    track_floating_glic_instance_ = false;
  }

  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  // These determine which glic instance is tracked by this class. This affects
  // many functions in this fixture. Only one will be present at a time.
  std::optional<InstanceId> tracked_instance_id_;
  std::optional<int> glic_instance_tab_index_ = 0;
  std::optional<tabs::TabInterface::Handle> glic_instance_tab_handle_;
  bool track_floating_glic_instance_ = false;
  std::optional<GURL> glic_fre_url_;

  base::WeakPtr<Browser> active_browser_;
  glic::GlicTestEnvironment glic_test_environment_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  // This is the default test file. Tests can override with a different path.
  std::string glic_page_path_ = "/glic/test_client/index.html";
  GURL guest_url_;

  base::test::ScopedFeatureList features_;

  std::map<std::string, std::string> mock_glic_query_params_;
};

// For most tests, you can alias or inherit from this instead of deriving your
// own `InteractiveGlicTestMixin<...>`.
using InteractiveGlicTest = InteractiveGlicTestMixin<InteractiveBrowserTest>;

// For testing IPH associated with glic - i.e. help bubbles that anchor in the
// chrome browser rather than showing up in the glic content itself - inherit
// from this.
using InteractiveGlicFeaturePromoTest =
    InteractiveGlicTestMixin<InteractiveFeaturePromoTest>;

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
