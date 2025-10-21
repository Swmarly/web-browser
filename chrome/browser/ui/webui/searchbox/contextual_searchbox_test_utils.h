// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_

#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/unguessable_token.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/composebox/composebox_metrics_recorder.h"
#include "components/omnibox/composebox/test_composebox_query_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}

namespace signin {
class IdentityManager;
}

class FakeVariationsClient;
class TemplateURLService;

class MockQueryController : public TestComposeboxQueryController {
 public:
  MockQueryController(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel,
      std::string locale,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      std::unique_ptr<QueryControllerConfigParams>
          query_controller_config_params);
  ~MockQueryController() override;

  MOCK_METHOD(void, NotifySessionStarted, (), (override));
  MOCK_METHOD(void, NotifySessionAbandoned, (), (override));
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input,
               std::optional<lens::ImageEncodingOptions> image_options),
              (override));
  MOCK_METHOD(bool, DeleteFile, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, ClearFiles, (), (override));
  MOCK_METHOD(FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken& file_token),
              (override));

  void NotifySessionStartedBase() {
    TestComposeboxQueryController::NotifySessionStarted();
  }
};

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  // WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
};

class MockComposeboxMetricsRecorder : public ComposeboxMetricsRecorder {
 public:
  MockComposeboxMetricsRecorder();
  ~MockComposeboxMetricsRecorder() override;

  MOCK_METHOD(void,
              NotifySessionStateChanged,
              (composebox::SessionState session_state),
              (override));
};

class ContextualSearchboxHandlerTestHarness
    : public ChromeRenderViewHostTestHarness {
 public:
  ContextualSearchboxHandlerTestHarness();
  ~ContextualSearchboxHandlerTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  TemplateURLService* template_url_service() { return template_url_service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return shared_url_loader_factory_;
  }
  FakeVariationsClient* fake_variations_client() {
    return fake_variations_client_.get();
  }

  ntp_composebox::FeatureConfig& scoped_config() {
    return scoped_config_.Get();
  }
  TestingProfile::TestingFactories GetTestingFactories() const override;

 private:
  ntp_composebox::ScopedFeatureConfigForTesting scoped_config_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<FakeVariationsClient> fake_variations_client_;
  base::HistogramTester histogram_tester_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_TEST_UTILS_H_
