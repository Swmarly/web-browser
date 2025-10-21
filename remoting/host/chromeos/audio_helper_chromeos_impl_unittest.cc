// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/audio_helper_chromeos_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "remoting/proto/audio.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = kSampleRate / 100;

media::AudioParameters GetTestAudioParams() {
  return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                kSampleRate, kFramesPerBuffer);
}

}  // namespace

class FakeAudioInputStream : public media::AudioInputStream {
 public:
  using OpenOutcome = media::AudioInputStream::OpenOutcome;

  FakeAudioInputStream() = default;

  void SetOpenOutcome(OpenOutcome outcome) { open_outcome_ = outcome; }
  bool IsStarted() const { return callback_ != nullptr; }
  bool IsClosed() const { return closed_; }

  void SimulateData(const media::AudioBus* audio_bus,
                    base::TimeTicks capture_time,
                    double volume) {
    callback_->OnData(audio_bus, capture_time, volume, {});
  }

  void SimulateError() { callback_->OnError(); }

  OpenOutcome Open() override { return open_outcome_; }

  void Start(AudioInputCallback* callback) override { callback_ = callback; }

  void Stop() override { callback_ = nullptr; }

  void Close() override {
    Stop();
    closed_ = true;
  }

  // Unused but required for overrides.
  double GetMaxVolume() override { return 0; }
  void SetVolume(double volume) override {}
  double GetVolume() override { return 0; }
  bool SetAutomaticGainControl(bool enabled) override { return false; }
  bool GetAutomaticGainControl() override { return false; }
  bool IsMuted() override { return false; }
  void SetOutputDeviceForAec(const std::string& input_device_id) override {}

 private:
  raw_ptr<AudioInputCallback> callback_ = nullptr;
  OpenOutcome open_outcome_ = OpenOutcome::kSuccess;
  bool closed_ = false;
};

class CustomFakeAudioManager : public media::FakeAudioManager {
 public:
  CustomFakeAudioManager()
      : media::FakeAudioManager(std::make_unique<media::TestAudioThread>(),
                                /*audio_log_factory=*/nullptr) {}
  ~CustomFakeAudioManager() override = default;

  media::AudioInputStream* MakeAudioInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override {
    if (fail_stream_creation_) {
      return nullptr;
    }

    params_ = params;
    device_id_ = device_id;
    input_stream_ = std::make_unique<FakeAudioInputStream>();

    if (fail_stream_open_) {
      input_stream_->SetOpenOutcome(
          media::AudioInputStream::OpenOutcome::kFailed);
    }

    return input_stream_.get();
  }

  void SetFailStreamCreation(bool fail) { fail_stream_creation_ = fail; }

  void SetFailStreamOpen(bool fail) { fail_stream_open_ = fail; }

  FakeAudioInputStream* GetInputStream() { return input_stream_.get(); }

  media::AudioParameters params() { return params_; }

  std::string device_id() { return device_id_; }

 private:
  std::unique_ptr<FakeAudioInputStream> input_stream_;
  bool fail_stream_creation_ = false;
  bool fail_stream_open_ = false;
  media::AudioParameters params_;
  std::string device_id_;
};

class AudioHelperChromeOsImplTest : public testing::Test {
 public:
  AudioHelperChromeOsImplTest() = default;

  ~AudioHelperChromeOsImplTest() override = default;

  void SetUp() override {
    audio_manager_ = std::make_unique<CustomFakeAudioManager>();
    audio_helper_chromeos_ = std::make_unique<AudioHelperChromeOsImpl>();
  }

  void TearDown() override {
    // AudioHelperChromeOS needs to be destroyed before the AudioManager because
    // on destruction, AudioHelperChromeOS attempts to close the audio stream.
    audio_helper_chromeos_.reset();
    audio_manager_->Shutdown();
  }

  void OnDataCallback(std::unique_ptr<AudioPacket> packet) {
    captured_audio_packets_.push_back(std::move(packet));
  }

  void OnErrorCallback() { ++on_error_called_count_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CustomFakeAudioManager> audio_manager_;
  std::unique_ptr<AudioHelperChromeOsImpl> audio_helper_chromeos_;
  std::vector<std::unique_ptr<AudioPacket>> captured_audio_packets_;
  int on_data_called_count_ = 0;
  int on_error_called_count_ = 0;
};

TEST_F(AudioHelperChromeOsImplTest, SuccessfulStartWithPackets) {
  audio_helper_chromeos_->StartAudioStream(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnDataCallback,
                          base::Unretained(this)),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnErrorCallback,
                          base::Unretained(this)));

  auto audio_bus = media::AudioBus::Create(GetTestAudioParams());
  audio_bus->Zero();
  base::TimeTicks capture_time = base::TimeTicks::Now();
  audio_manager_->GetInputStream()->SimulateData(audio_bus.get(), capture_time,
                                                 /* volume= */ 50);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return captured_audio_packets_.size() == 1; }));

  const auto& packet = captured_audio_packets_[0];
  EXPECT_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  EXPECT_EQ(AudioPacket::SAMPLING_RATE_48000, packet->sampling_rate());
  EXPECT_EQ(AudioPacket::BYTES_PER_SAMPLE_2, packet->bytes_per_sample());
  EXPECT_EQ(AudioPacket::CHANNELS_STEREO, packet->channels());
}

TEST_F(AudioHelperChromeOsImplTest, VerifyStreamParams) {
  audio_helper_chromeos_->StartAudioStream(
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing(),
      base::DoNothing());

  EXPECT_EQ(media::AudioDeviceDescription::kLoopbackWithMuteDeviceId,
            audio_manager_->device_id());
  EXPECT_EQ(GetTestAudioParams().sample_rate(),
            audio_manager_->params().sample_rate());
  EXPECT_EQ(GetTestAudioParams().channels(),
            audio_manager_->params().channels());
  EXPECT_EQ(GetTestAudioParams().frames_per_buffer(),
            audio_manager_->params().frames_per_buffer());
  EXPECT_EQ(GetTestAudioParams().format(), audio_manager_->params().format());
}

TEST_F(AudioHelperChromeOsImplTest, SuccessfulStartWithStreamFailure) {
  audio_helper_chromeos_->StartAudioStream(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnDataCallback,
                          base::Unretained(this)),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnErrorCallback,
                          base::Unretained(this)));
  EXPECT_TRUE(audio_manager_->GetInputStream()->IsStarted());

  audio_manager_->GetInputStream()->SimulateError();

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return on_error_called_count_ == 1; }));
}

TEST_F(AudioHelperChromeOsImplTest, FailedStartStreamNotCreated) {
  audio_manager_->SetFailStreamCreation(/* fail= */ true);

  audio_helper_chromeos_->StartAudioStream(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnDataCallback,
                          base::Unretained(this)),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnErrorCallback,
                          base::Unretained(this)));

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return on_error_called_count_ == 1; }));
}

TEST_F(AudioHelperChromeOsImplTest, FailedStartStreamNotOpened) {
  audio_manager_->SetFailStreamOpen(/* fail= */ true);

  audio_helper_chromeos_->StartAudioStream(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnDataCallback,
                          base::Unretained(this)),
      base::BindRepeating(&AudioHelperChromeOsImplTest::OnErrorCallback,
                          base::Unretained(this)));

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return on_error_called_count_ == 1; }));
}

}  // namespace remoting
