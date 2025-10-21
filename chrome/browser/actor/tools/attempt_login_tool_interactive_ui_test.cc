// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace actor {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

const SkBitmap GenerateSquareBitmap(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(size, size, kOpaque_SkAlphaType));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap;
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(Profile* profile) : ExecutionEngine(profile) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(actor_login::ActorLoginService&,
              GetActorLoginService,
              (),
              (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
};

using AttemptLoginToolInteractiveUiTestBase =
    InteractiveBrowserTestMixin<ActorToolsTest>;

// TODO(crbug.com/441533831): We should migrate the Javascript tests to
// typescript.
class AttemptLoginToolInteractiveUiTest
    : public glic::test::InteractiveGlicTestMixin<
          AttemptLoginToolInteractiveUiTestBase> {
 public:
  AttemptLoginToolInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{password_manager::features::kActorLogin,
                              actor::kGlicEnableAutoLoginDialogs},
        /*disabled_features=*/{});
  }
  ~AttemptLoginToolInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTestMixin<
        AttemptLoginToolInteractiveUiTestBase>::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());

    ON_CALL(mock_execution_engine(), GetActorLoginService())
        .WillByDefault(ReturnRef(mock_login_service_));

    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(&mock_favicon_service_));

    ON_CALL(mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
        .WillByDefault([this](const GURL& page_url,
                              favicon_base::FaviconImageCallback callback,
                              base::CancelableTaskTracker* tracker) {
          favicon_base::FaviconImageResult result;
          result.image = red_image_;
          std::move(callback).Run(std::move(result));
          return static_cast<base::CancelableTaskTracker::TaskId>(1);
        });
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Must enable the pixel output. Otherwise the PNG icons will not be
    // rendered.
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    glic::test::InteractiveGlicTestMixin<
        AttemptLoginToolInteractiveUiTestBase>::SetUpCommandLine(command_line);
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile) override {
    return std::make_unique<NiceMock<MockExecutionEngine>>(profile);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  actor_login::Credential::Id GenerateCredentialId() {
    return credential_id_generator_.GenerateNextId();
  }

  const SkBitmap& red_bitmap() { return red_bitmap_; }

 private:
  const SkBitmap red_bitmap_ = GenerateSquareBitmap(/*size=*/10, SK_ColorRED);
  const gfx::Image red_image_ = gfx::Image::CreateFrom1xBitmap(red_bitmap_);

  MockActorLoginService mock_login_service_;
  favicon::MockFaviconService mock_favicon_service_;

  base::test::ScopedFeatureList scoped_feature_list_;

  actor_login::Credential::Id::Generator credential_id_generator_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AttemptLoginToolInteractiveUiTest, SmokeTest) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login),
      MakeTestCredential(u"username2", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Toggle the glic window.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      InAnyContext(WithElement(
          glic::test::kGlicContentsElementId,
          [](::ui::TrackedElement* el) mutable {
            static constexpr char kHandleDialogRequest[] =
                R"js(
      (() => {
        /** Converts a PNG (Blob) to a base64 encoded string. */
        function blobToBase64(blob) {
          return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onloadend = () => {
              resolve(reader.result);
            };
            reader.onerror = reject;
            reader.readAsDataURL(blob);
          });
        }

        window.credentialDialogRequestData = new Promise(resolve => {
          client.browser.selectCredentialDialogRequestHandler().subscribe(
            async (request) => {
              // Respond to the request by selecting the second credential.
              request.onDialogClosed({
                response: {
                  taskId: request.taskId,
                  selectedCredentialId: request.credentials[1].id,
                }
              });

              const credentialsWithIcons = await Promise.all(
                request.credentials.map(async (cred) => {
                  const {getIcon, ...rest} = cred;
                  if (!getIcon) {
                    return rest;
                  }
                  const blob = await getIcon();
                  if (!blob) {
                    return rest;
                  }
                  const icon = await blobToBase64(blob);
                  return {...rest, icon};
                })
              );

              // Resolve the promise with the request data to be verified in
              // C++.
              resolve({
                taskId: request.taskId,
                showDialog: request.showDialog,
                credentials: credentialsWithIcons,
              });
            }
          );
        });
      })();
              )js";
            content::WebContents* glic_contents =
                AsInstrumentedWebContents(el)->web_contents();
            ASSERT_TRUE(content::ExecJs(glic_contents, kHandleDialogRequest));
          })));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // The ActResultFuture `result` will be resolved in a RunLoop of kDefault. It
  // shouldn't be placed inside `RunTestSequence()`.
  ExpectOkResult(result);

  // Note that the URL here is actually different from the URL for
  // `red_bitmap()`. The differences are the metadata but not the pixel values.
  // The test will compare the pixel values.
  constexpr char kExpectedIconBase64Url[] =
      "iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+"
      "9AAAAI0lEQVR4AeyQMQ0AAAyDSP177hwsCCgJHxcp1BgkC99Res8BAAD//"
      "+wxhQIAAAAGSURBVAMAZIwUAbOgDh0AAAAASUVORK5CYII=";
  const std::string kExpectedIconDataUrl =
      base::StrCat({"data:image/png;base64,", kExpectedIconBase64Url});
  auto expected_request =
      base::Value::Dict()
          .Set("taskId", actor_task().id().value())
          .Set("showDialog", true)
          .Set("credentials",
               base::Value::List()
                   .Append(base::Value::Dict()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username1")
                               .Set("sourceSiteOrApp",
                                    url.GetWithEmptyPath().spec())
                               .Set("icon", kExpectedIconDataUrl))
                   .Append(base::Value::Dict()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username2")
                               .Set("sourceSiteOrApp",
                                    url.GetWithEmptyPath().spec())
                               .Set("icon", kExpectedIconDataUrl)));

  // Verify the dialog request content.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        static constexpr char kGetRequestData[] =
            R"js(
              (() => {
                return window.credentialDialogRequestData;
              })();
            )js";
        auto eval_result = content::EvalJs(glic_contents, kGetRequestData);
        const auto& actual_request = eval_result.ExtractDict();
        ASSERT_EQ(expected_request, actual_request);
        // Decode the icon from the web client, and compare the pixel values.
        std::optional<std::vector<uint8_t>> decoded_icon =
            base::Base64Decode(kExpectedIconBase64Url);
        ASSERT_TRUE(decoded_icon.has_value());
        ASSERT_TRUE(cc::MatchesBitmap(red_bitmap(),
                                      gfx::PNGCodec::Decode(*decoded_icon),
                                      cc::ExactPixelComparator()));
      })));

  // We selected the second credential in the dialog.
  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username2", last_credential_used->username);
}

}  // namespace actor
