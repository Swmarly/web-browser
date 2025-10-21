// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_message_channel_strategy.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/base/http_status.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/ftl_services_context.h"
#include "remoting/signaling/message_channel.h"
#include "remoting/signaling/signaling_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Expectation;
using ::testing::Field;
using ::testing::Return;

using remoting::internal::ChannelActiveStruct;
using remoting::internal::ChannelOpenStruct;
using remoting::internal::ReceiveClientMessagesResponseStruct;
using remoting::internal::SimpleMessageStruct;

using MessageReceivedCallback =
    CorpMessageChannelStrategy::MessageReceivedCallback;
using StatusCallback = base::OnceCallback<void(const HttpStatus&)>;

// TODO: joedow - Move `FtlServicesContext` constants to a shared file.
constexpr base::TimeDelta kTestBackoffInitialDelay =
    FtlServicesContext::kBackoffInitialDelay;
constexpr base::TimeDelta kTestBackoffMaxDelay =
    FtlServicesContext::kBackoffMaxDelay;
constexpr base::TimeDelta kInactivityTimeout = base::Seconds(15);

std::unique_ptr<ReceiveClientMessagesResponseStruct>
CreateChannelActiveMessage() {
  auto response = std::make_unique<ReceiveClientMessagesResponseStruct>();
  response->message.emplace<ChannelActiveStruct>();
  return response;
}

std::unique_ptr<ReceiveClientMessagesResponseStruct>
CreateChannelOpenMessage() {
  auto response = std::make_unique<ReceiveClientMessagesResponseStruct>();
  response->message.emplace<ChannelOpenStruct>(
      ChannelOpenStruct{.channel_lifetime = base::Minutes(15),
                        .inactivity_timeout = kInactivityTimeout});
  return response;
}

std::unique_ptr<ReceiveClientMessagesResponseStruct> CreateSimpleMessage(
    std::string message_payload) {
  SimpleMessageStruct simple_message;
  simple_message.message_id = "42";
  simple_message.payload = std::move(message_payload);
  simple_message.create_time = base::Time::Now();
  auto response = std::make_unique<ReceiveClientMessagesResponseStruct>();
  response->message.emplace<SimpleMessageStruct>(std::move(simple_message));
  return response;
}

class MockSignalingTracker : public SignalingTracker {
 public:
  MOCK_METHOD(void, OnSignalingActive, (), (override));
};

// Fake stream implementation to allow probing if a stream is closed by client.
class FakeScopedProtobufHttpRequest : public ScopedProtobufHttpRequest {
 public:
  FakeScopedProtobufHttpRequest()
      : ScopedProtobufHttpRequest(base::DoNothing()) {}

  FakeScopedProtobufHttpRequest(const FakeScopedProtobufHttpRequest&) = delete;
  FakeScopedProtobufHttpRequest& operator=(
      const FakeScopedProtobufHttpRequest&) = delete;

  ~FakeScopedProtobufHttpRequest() override = default;

  base::WeakPtr<FakeScopedProtobufHttpRequest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeScopedProtobufHttpRequest> weak_factory_{this};
};

std::unique_ptr<FakeScopedProtobufHttpRequest> CreateFakeServerStream() {
  return std::make_unique<FakeScopedProtobufHttpRequest>();
}

// Creates a gmock EXPECT_CALL action that:
//   1. Creates a fake server stream and returns it as the start stream result
//   2. Posts a task to call |on_stream_opened| at the end of current sequence
//   3. Writes the WeakPtr to the fake server stream to |optional_out_stream|
//      if it is provided.
template <typename OnStreamOpenedLambda>
decltype(auto) StartStream(OnStreamOpenedLambda on_stream_opened,
                           base::WeakPtr<FakeScopedProtobufHttpRequest>*
                               optional_out_stream = nullptr) {
  return [=](base::OnceClosure on_channel_ready,
             const MessageReceivedCallback& on_incoming_msg,
             StatusCallback on_channel_closed) {
    auto fake_stream = CreateFakeServerStream();
    if (optional_out_stream) {
      *optional_out_stream = fake_stream->GetWeakPtr();
    }
    auto on_stream_opened_cb = base::BindLambdaForTesting(on_stream_opened);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(on_stream_opened_cb, std::move(on_channel_ready),
                       on_incoming_msg, std::move(on_channel_closed)));
    return fake_stream;
  };
}

base::OnceClosure NotReachedClosure() {
  return base::BindOnce([]() { NOTREACHED(); });
}

base::RepeatingCallback<void(const HttpStatus&)> NotReachedStatusCallback(
    const base::Location& location) {
  return base::BindLambdaForTesting([=](const HttpStatus& status) {
    NOTREACHED() << "Location: " << location.ToString()
                 << ", status code: " << static_cast<int>(status.error_code());
  });
}

base::OnceCallback<void(const HttpStatus&)> CheckStatusThenQuitRunLoopCallback(
    const base::Location& from_here,
    HttpStatus::Code expected_status_code,
    base::RunLoop* run_loop) {
  return base::BindLambdaForTesting([=](const HttpStatus& status) {
    ASSERT_EQ(expected_status_code, status.error_code())
        << "Incorrect status code. Location: " << from_here.ToString();
    run_loop->QuitWhenIdle();
  });
}

}  // namespace

class CorpMessageChannelStrategyTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  base::TimeDelta GetTimeUntilRetry() const;
  int GetRetryFailureCount() const;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MessageChannel> channel_;
  base::MockCallback<CorpMessageChannelStrategy::StreamOpener>
      mock_stream_opener_;
  base::MockCallback<base::RepeatingCallback<void(const SimpleMessageStruct&)>>
      mock_on_incoming_msg_;
  MockSignalingTracker mock_signaling_tracker_;
  raw_ptr<CorpMessageChannelStrategy> raw_strategy_;
};

void CorpMessageChannelStrategyTest::SetUp() {
  auto strategy = std::make_unique<CorpMessageChannelStrategy>();
  raw_strategy_ = strategy.get();
  strategy->Initialize(mock_stream_opener_.Get(), mock_on_incoming_msg_.Get());
  channel_ = std::make_unique<MessageChannel>(std::move(strategy),
                                              &mock_signaling_tracker_);
}

void CorpMessageChannelStrategyTest::TearDown() {
  raw_strategy_ = nullptr;
  channel_.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

base::TimeDelta CorpMessageChannelStrategyTest::GetTimeUntilRetry() const {
  return channel_->GetReconnectRetryBackoffEntryForTesting()
      .GetTimeUntilRelease();
}

int CorpMessageChannelStrategyTest::GetRetryFailureCount() const {
  return channel_->GetReconnectRetryBackoffEntryForTesting().failure_count();
}

TEST_F(CorpMessageChannelStrategyTest,
       TestStartReceivingMessages_StoppedImmediately) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        channel_->StopReceivingMessages();
        run_loop.Quit();
      }));

  channel_->StartReceivingMessages(NotReachedClosure(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       TestStartReceivingMessages_NotAuthenticated) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(
          StartStream([&](base::OnceClosure on_channel_ready,
                          const MessageReceivedCallback& on_incoming_msg,
                          StatusCallback on_channel_closed) {
            std::move(on_channel_closed)
                .Run(HttpStatus(HttpStatus::Code::UNAUTHENTICATED, ""));
          }));

  channel_->StartReceivingMessages(
      NotReachedClosure(),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, HttpStatus::Code::UNAUTHENTICATED, &run_loop));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       TestStartReceivingMessages_StreamStarted) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        on_incoming_msg.Run(CreateChannelOpenMessage());

        std::move(on_channel_ready).Run();
      }));

  channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       TestStartReceivingMessages_RecoverableStreamError) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  base::WeakPtr<FakeScopedProtobufHttpRequest> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const MessageReceivedCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // The first open stream attempt fails with UNAVAILABLE error.
            ASSERT_EQ(GetRetryFailureCount(), 0);

            std::move(on_channel_closed)
                .Run(HttpStatus(HttpStatus::Code::UNAVAILABLE, ""));

            ASSERT_EQ(GetRetryFailureCount(), 1);
            ASSERT_NEAR(kTestBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);

            // This will make the channel reopen the stream.
            task_environment_.FastForwardBy(GetTimeUntilRetry());
          },
          &old_stream))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        // Second open stream attempt succeeds.

        // Assert old stream closed.
        ASSERT_FALSE(old_stream);

        on_incoming_msg.Run(CreateChannelOpenMessage());

        std::move(on_channel_ready).Run();

        ASSERT_EQ(GetRetryFailureCount(), 0);
      }));

  channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       TestStartReceivingMessages_MultipleCalls) {
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> stream_ready_callback;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  // Exits the run loop iff the callback is called three times with OK.
  EXPECT_CALL(stream_ready_callback, Run())
      .WillOnce(Return())
      .WillOnce(Return())
      .WillOnce([&]() { run_loop.Quit(); });

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        on_incoming_msg.Run(CreateChannelOpenMessage());
        std::move(on_channel_ready).Run();
      }));

  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));
  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));
  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest, StreamsTwoMessages) {
  base::RunLoop run_loop;

  constexpr char kMessage1Payload[] = "msg_1_payload";
  constexpr char kMessage2Payload[] = "msg_2_payload";

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  EXPECT_CALL(mock_on_incoming_msg_,
              Run(Field(&SimpleMessageStruct::payload, kMessage1Payload)))
      .WillOnce(Return());
  EXPECT_CALL(mock_on_incoming_msg_,
              Run(Field(&SimpleMessageStruct::payload, kMessage2Payload)))
      .WillOnce([&](const SimpleMessageStruct&) { run_loop.Quit(); });

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        on_incoming_msg.Run(CreateChannelOpenMessage());
        std::move(on_channel_ready).Run();

        on_incoming_msg.Run(CreateSimpleMessage(kMessage1Payload));
        on_incoming_msg.Run(CreateSimpleMessage(kMessage2Payload));

        const HttpStatus kCancel(HttpStatus::Code::CANCELLED, "Cancelled");
        std::move(on_channel_closed).Run(kCancel);
      }));

  channel_->StartReceivingMessages(
      base::DoNothing(),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, HttpStatus::HttpStatus::Code::CANCELLED, &run_loop));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       ReceivedOneKeepAlive_OnSignalingActiveTwice) {
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> stream_ready_callback;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive())
      .WillOnce(Return())
      .WillOnce([&]() { run_loop.Quit(); });

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        on_incoming_msg.Run(CreateChannelOpenMessage());

        std::move(on_channel_ready).Run();

        on_incoming_msg.Run(CreateChannelActiveMessage());
      }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest, NoKeepAliveWithinTimeout_ResetsStream) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive())
      .Times(2)
      .WillRepeatedly(Return());

  base::WeakPtr<FakeScopedProtobufHttpRequest> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const MessageReceivedCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            on_incoming_msg.Run(CreateChannelOpenMessage());

            std::move(on_channel_ready).Run();

            task_environment_.FastForwardBy(
                raw_strategy_->GetInactivityTimeout());

            ASSERT_EQ(GetRetryFailureCount(), 1);
            ASSERT_NEAR(kTestBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);

            // This will make the channel reopen the stream.
            task_environment_.FastForwardBy(GetTimeUntilRetry());
          },
          &old_stream))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        // Stream is reopened.

        // Assert old stream closed.
        ASSERT_FALSE(old_stream);

        on_incoming_msg.Run(CreateChannelOpenMessage());

        std::move(on_channel_ready).Run();
        ASSERT_EQ(GetRetryFailureCount(), 0);
        run_loop.Quit();
      }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest, ServerClosesStream_ResetsStream) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive())
      .Times(2)
      .WillRepeatedly(Return());

  base::WeakPtr<FakeScopedProtobufHttpRequest> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const MessageReceivedCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            auto fake_server_stream = CreateFakeServerStream();

            on_incoming_msg.Run(CreateChannelOpenMessage());
            std::move(on_channel_ready).Run();

            // Close the stream with OK.
            std::move(on_channel_closed).Run(HttpStatus::OK());
          },
          &old_stream))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        ASSERT_FALSE(old_stream);

        std::move(on_channel_ready).Run();
        ASSERT_EQ(GetRetryFailureCount(), 0);
        run_loop.Quit();
      }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest, TimeoutIncreasesToMaximum) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  int failure_count = 0;
  int hitting_max_delay_count = 0;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillRepeatedly(
          StartStream([&](base::OnceClosure on_channel_ready,
                          const MessageReceivedCallback& on_incoming_msg,
                          StatusCallback on_channel_closed) {
            // Quit if delay is ~kBackoffMaxDelay three times.
            if (hitting_max_delay_count == 3) {
              on_incoming_msg.Run(CreateChannelOpenMessage());
              std::move(on_channel_ready).Run();
              ASSERT_EQ(0, GetRetryFailureCount());
              run_loop.Quit();
              return;
            }

            // Otherwise send UNAVAILABLE to reset the stream.

            std::move(on_channel_closed)
                .Run(HttpStatus(HttpStatus::HttpStatus::Code::UNAVAILABLE, ""));

            int new_failure_count = GetRetryFailureCount();
            ASSERT_LT(failure_count, new_failure_count);
            failure_count = new_failure_count;

            base::TimeDelta time_until_retry = GetTimeUntilRetry();

            base::TimeDelta max_delay_diff =
                time_until_retry - kTestBackoffMaxDelay;

            // Adjust for fuzziness.
            if (max_delay_diff.magnitude() < base::Milliseconds(500)) {
              hitting_max_delay_count++;
            }

            // This will tail-recursively call the stream opener.
            task_environment_.FastForwardBy(time_until_retry);
          }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(CorpMessageChannelStrategyTest,
       StartStreamFailsWithUnRecoverableErrorAndRetry_TimeoutApplied) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_signaling_tracker_, OnSignalingActive()).WillOnce(Return());

  base::WeakPtr<FakeScopedProtobufHttpRequest> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const MessageReceivedCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // The first open stream attempt fails with UNAUTHENTICATED error.
            ASSERT_EQ(GetRetryFailureCount(), 0);

            std::move(on_channel_closed)
                .Run(HttpStatus(HttpStatus::Code::UNAUTHENTICATED, ""));

            ASSERT_EQ(GetRetryFailureCount(), 1);
            ASSERT_NEAR(kTestBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);
          },
          &old_stream))
      .WillOnce(StartStream([&](base::OnceClosure on_channel_ready,
                                const MessageReceivedCallback& on_incoming_msg,
                                StatusCallback on_channel_closed) {
        // Second open stream attempt succeeds.

        // Assert old stream closed.
        ASSERT_FALSE(old_stream);
        ASSERT_EQ(GetRetryFailureCount(), 1);

        on_incoming_msg.Run(CreateChannelOpenMessage());

        std::move(on_channel_ready).Run();
        ASSERT_EQ(GetRetryFailureCount(), 0);
      }));

  channel_->StartReceivingMessages(
      base::DoNothing(),
      base::BindLambdaForTesting([&](const HttpStatus& status) {
        ASSERT_EQ(status.error_code(), HttpStatus::Code::UNAUTHENTICATED);
        channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                         NotReachedStatusCallback(FROM_HERE));
      }));

  run_loop.Run();
}

}  // namespace remoting
