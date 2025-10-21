// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/unexportable_keys/features.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_usage.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

using net::device_bound_sessions::SessionAccess;
using net::device_bound_sessions::SessionKey;

namespace {

class DeviceBoundSessionAccessObserver : public content::WebContentsObserver {
 public:
  DeviceBoundSessionAccessObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void(const SessionAccess&)> on_access_callback)
      : WebContentsObserver(web_contents),
        on_access_callback_(std::move(on_access_callback)) {}

  void OnDeviceBoundSessionAccessed(content::NavigationHandle* navigation,
                                    const SessionAccess& access) override {
    on_access_callback_.Run(access);
  }
  void OnDeviceBoundSessionAccessed(content::RenderFrameHost* rfh,
                                    const SessionAccess& access) override {
    on_access_callback_.Run(access);
  }

 private:
  base::RepeatingCallback<void(const SessionAccess&)> on_access_callback_;
};

class DeviceBoundSessionBrowserTest : public InProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  DeviceBoundSessionBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {net::features::kDeviceBoundSessions,
         {{"OriginTrialFeedback", GetParam() ? "true" : "false"}}},
        {unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting,
         {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    EXPECT_TRUE(embedded_https_test_server().InitializeAndListen());
    embedded_https_test_server().RegisterRequestHandler(
        net::device_bound_sessions::GetTestRequestHandler(GetURL("/")));
    embedded_https_test_server().StartAcceptingConnections();
  }

  GURL GetURL(std::string_view relative_url) {
    // We use one of the SSL certificates configured by CERT_TEST_NAMES
    // so we can do a DBSC session in a secure context.
    return embedded_https_test_server().GetURL("a.test", relative_url);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        net::device_bound_sessions::kTestOriginTrialPublicKey);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, DeviceBoundSessionBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest,
                       AccessCalledOnRegistrationFromNavigation) {
  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/dbsc_login_page")));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "document.location = \"/dbsc_required\""));

  SessionAccess access = future.Take();
  EXPECT_EQ(access.session_key.site, net::SchemefulSite(GetURL("/")));
  EXPECT_EQ(access.session_key.id, SessionKey::Id("session_id"));
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest,
                       AccessCalledOnRegistrationFromResource) {
  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("/resource_triggered_dbsc_registration")));

  SessionAccess access = future.Take();
  EXPECT_EQ(access.session_key.site, net::SchemefulSite(GetURL("/")));
  EXPECT_EQ(access.session_key.id, SessionKey::Id("session_id"));

  EXPECT_THAT(GetCanonicalCookies(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext(),
                                  GetURL("/dbsc_required")),
              testing::Contains(net::MatchesCookieWithName("auth_cookie")));
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest, UseCounterOnNavigation) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/dbsc_login_page")));
  ASSERT_TRUE(
      content::ExecJs(web_contents, "document.location = \"/dbsc_required\""));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRegistered),
            1);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest, UseCounterOnResource) {
  WebFeatureHistogramTester histograms;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRegistered),
            1);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest,
                       UseCounterForNotDeferred) {
  WebFeatureHistogramTester histograms;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/ensure_authenticated")));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestDeferral),
            0);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest, UseCounterForDeferred) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/ensure_authenticated")));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestDeferral),
            1);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest,
                       UseCounterForMultipleRequestsOnePage) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Make several requests with JS
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only one use counter
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            1);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest,
                       UseCounterForMultipleRequestsTwoPages) {
  WebFeatureHistogramTester histograms;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Make several requests with JS
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate again
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/ensure_authenticated")));

  // Make several more in-scope requests
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));
  ASSERT_TRUE(content::ExecJs(web_contents, "fetch('/ensure_authenticated')"));

  // Navigate away in order to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect two use counters, one for each page load
  EXPECT_EQ(histograms.GetCount(
                blink::mojom::WebFeature::kDeviceBoundSessionRequestInScope),
            2);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest, NotDeferredLogs) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<SessionAccess> future;
  DeviceBoundSessionAccessObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetRepeatingCallback<const SessionAccess&>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("/resource_triggered_dbsc_registration")));

  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/ensure_authenticated")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision2",
      /*sample=*/net::device_bound_sessions::SessionUsage::kInScopeNotDeferred,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(DeviceBoundSessionBrowserTest, DeferredLogs) {
  base::HistogramTester histogram_tester;

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  {
    base::test::TestFuture<SessionAccess> future;
    DeviceBoundSessionAccessObserver observer(
        web_contents, future.GetRepeatingCallback<const SessionAccess&>());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GetURL("/resource_triggered_dbsc_registration")));
    ASSERT_TRUE(future.Wait());
  }

  // Force a refresh.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "cookieStore.delete('auth_cookie')"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("/ensure_authenticated")));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.RequestDeferralDecision2",
      /*sample=*/net::device_bound_sessions::SessionUsage::kDeferred,
      /*expected_count=*/1);
}

}  // namespace
