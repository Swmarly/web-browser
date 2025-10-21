// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include "base/byte_count.h"
#include "base/functional/bind.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace memory_pressure {
namespace win {

namespace {

struct PressureSettings {
  base::ByteCount phys_left;
  base::MemoryPressureLevel level;
};

constexpr char kCommitLimitMBHistogramName[] = "Memory.CommitLimitMB";
constexpr char kCommitAvailableMBHistogramName[] = "Memory.CommitAvailableMB";
constexpr char kCommitPercentageUsedHistogramName[] =
    "Memory.CommitPercentageUsed";

}  // namespace

// This is outside of the anonymous namespace so that it can be seen as a friend
// to the evaluator class.
class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel;
  using SystemMemoryPressureEvaluator::CheckMemoryPressure;
  using SystemMemoryPressureEvaluator::RecordCommitHistograms;

  explicit TestSystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)), mem_status_() {
    // Generate a plausible amount of memory.
    mem_status_.ullTotalPhys = base::MiB(8000).InBytesUnsigned();

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(base::ByteCount system_memory,
                                    base::ByteCount moderate_threshold,
                                    base::ByteCount critical_threshold)
      : SystemMemoryPressureEvaluator(moderate_threshold,
                                      critical_threshold,
                                      nullptr),
        mem_status_() {
    // Set the amount of system memory.
    mem_status_.ullTotalPhys = system_memory.InBytesUnsigned();

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(base::ByteCount phys_left) {
    // ullTotalPhys is set in the constructor and not modified.

    // Set the amount of available memory.
    mem_status_.ullAvailPhys = phys_left.InBytesUnsigned();
    DCHECK_LT(mem_status_.ullAvailPhys, mem_status_.ullTotalPhys);

    // These fields are unused.
    mem_status_.dwMemoryLoad = 0;
    mem_status_.ullTotalVirtual = 0;
    mem_status_.ullAvailVirtual = 0;
  }

  // Sets up the memory status to reflect commit limit and available.
  void SetCommitData(base::ByteCount commit_limit,
                     base::ByteCount commit_available) {
    mem_status_.ullTotalPageFile = commit_limit.InBytesUnsigned();
    mem_status_.ullAvailPageFile = commit_available.InBytesUnsigned();
  }

  void SetNone() { SetMemoryFree(moderate_threshold() + base::MiB(1)); }

  void SetModerate() { SetMemoryFree(moderate_threshold() - base::MiB(1)); }

  void SetCritical() { SetMemoryFree(critical_threshold() - base::MiB(1)); }

  MEMORYSTATUSEX GetSystemMemoryStatusForTesting() { return mem_status_; }

 private:
  bool GetSystemMemoryStatus(MEMORYSTATUSEX* mem_status) override {
    // Simply copy the memory status set by the test fixture.
    *mem_status = mem_status_;
    return true;
  }

  MEMORYSTATUSEX mem_status_;
};

class WinSystemMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestSystemMemoryPressureEvaluator* evaluator) {
    base::ByteCount moderate = evaluator->moderate_threshold();
    evaluator->SetMemoryFree(moderate + base::MiB(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(moderate);
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(moderate - base::MiB(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    base::ByteCount critical = evaluator->critical_threshold();
    evaluator->SetMemoryFree(critical + base::MiB(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(critical);
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(critical - base::MiB(1));
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Tests the fundamental direct calculation of memory pressure with default
// thresholds.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelDefault) {
  static constexpr base::ByteCount kModerate =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultModerateThreshold;
  static constexpr base::ByteCount kCritical =
      SystemMemoryPressureEvaluator::kPhysicalMemoryDefaultCriticalThreshold;

  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static constexpr base::ByteCount kSystem = base::MiB(512);
  static constexpr base::ByteCount kModerate = base::MiB(256);
  static constexpr base::ByteCount kCritical = base::MiB(128);

  TestSystemMemoryPressureEvaluator evaluator(kSystem, kModerate, kCritical);

  EXPECT_EQ(kModerate, evaluator.moderate_threshold());
  EXPECT_EQ(kCritical, evaluator.critical_threshold());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// This test tests the various transition states from memory pressure, looking
// for the correct behavior on event reposting as well as state updates.
TEST_F(WinSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;

  TestSystemMemoryPressureEvaluator evaluator(monitor.CreateVoter());

  testing::StrictMock<base::RegisteredMockMemoryPressureListener> listener;

  // Checking the memory pressure at 0% load should not produce any
  // events.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());

  // Setting the memory level to 80% should produce a moderate pressure level.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after a while.
  const int kModeratePressureCooldownCycles =
      evaluator.kModeratePressureCooldown / evaluator.kDefaultPeriod;

  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Setting the memory usage to 99% should produce critical levels.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.SetCritical();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Calling it again should immediately produce a second call.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // When lowering the pressure again there should be a notification and the
  // pressure should go back to moderate.
  EXPECT_CALL(listener, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);

  // Check that the event gets reposted after a while.
  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(listener,
                  OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_MODERATE, evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&listener);
  }

  // Going down to no pressure should not produce an notification.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_NONE, evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&listener);
}

// RecordCommitHistograms emits the correct histograms when
// GetSystemMemoryStatus succeeds.
TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsBasic) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::GiB(4),
                          /*commit_available=*/base::GiB(2));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 4096, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 2048, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 50,
                                      1);
}

// Verifies behavior when commit limit is zero (division by zero).
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       RecordCommitHistogramsDivisionByZero) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::ByteCount(0),
                          /*commit_available=*/base::ByteCount(0));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

// RecordCommitHistograms should be able to handle commit values greater than
// 32-bit integers to calculate and correctly output all histograms.
TEST_F(WinSystemMemoryPressureEvaluatorTest, RecordCommitHistogramsOverflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  constexpr base::ByteCount kLargerThanMaxInt =
      base::MiB(static_cast<uint64_t>(std::numeric_limits<int>::max()) + 1U);
  evaluator.SetCommitData(/*commit_limit=*/kLargerThanMaxInt,
                          /*commit_available=*/kLargerThanMaxInt);

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 10000000, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 10000000,
                                      1);
}

// Verifies that RecordCommitHistograms correctly handles the calculation of
// Memory.CommitPercentageUsed, specifically addressing the potential for
// underflow in that calculation.
TEST_F(WinSystemMemoryPressureEvaluatorTest, PotentialUnderflow) {
  base::HistogramTester histogram_tester;
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.SetCommitData(/*commit_limit=*/base::MiB(50),
                          /*commit_available=*/base::MiB(100));

  evaluator.RecordCommitHistograms(evaluator.GetSystemMemoryStatusForTesting());

  histogram_tester.ExpectUniqueSample(kCommitLimitMBHistogramName, 50, 1);
  histogram_tester.ExpectUniqueSample(kCommitAvailableMBHistogramName, 100, 1);
  histogram_tester.ExpectUniqueSample(kCommitPercentageUsedHistogramName, 0, 1);
}

}  // namespace win
}  // namespace memory_pressure
