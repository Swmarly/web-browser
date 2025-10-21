// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/on_device_model/on_device_model_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "services/on_device_model/backend.h"
#include "services/on_device_model/fake/on_device_model_fake.h"
#include "services/on_device_model/ml/on_device_model_executor.h"
#include "services/on_device_model/on_device_model_mojom_impl.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"

namespace on_device_model {
namespace {

const base::FeatureParam<bool> kForceFastestInference{
    &optimization_guide::features::kOptimizationGuideOnDeviceModel,
    "on_device_model_force_fastest_inference", false};

std::unique_ptr<Backend> DefaultImpl() {
  if (base::FeatureList::IsEnabled(features::kUseFakeChromeML)) {
    return std::make_unique<ml::BackendImpl>(fake_ml::GetFakeChromeML());
  }
#if defined(ENABLE_ML_INTERNAL)
  return std::make_unique<ml::BackendImpl>(::ml::ChromeML::Get());
#else
  return std::make_unique<ml::BackendImpl>(fake_ml::GetFakeChromeML());
#endif  // defined(ENABLE_ML_INTERNAL)
}

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    const ml::ChromeML& chrome_ml)
    : receiver_(this, std::move(receiver)),
      backend_(std::make_unique<ml::BackendImpl>(&chrome_ml)) {}

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    std::unique_ptr<Backend> backend)
    : receiver_(this, std::move(receiver)), backend_(std::move(backend)) {}

OnDeviceModelService::~OnDeviceModelService() = default;

// static
std::unique_ptr<mojom::OnDeviceModelService> OnDeviceModelService::Create(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    std::unique_ptr<Backend> backend) {
  if (!backend) {
    backend = DefaultImpl();
  }
  RETURN_IF_ERROR(backend->CanCreate(),
                  [&](ServiceDisconnectReason reason)
                      -> std::unique_ptr<mojom::OnDeviceModelService> {
                    receiver.ResetWithReason(static_cast<uint32_t>(reason),
                                             "Error loading backend.");
                    return nullptr;
                  });
  // No errors, return real service.
  return std::make_unique<OnDeviceModelService>(std::move(receiver),
                                                std::move(backend));
}

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (kForceFastestInference.Get()) {
    params->performance_hint = ml::ModelPerformanceHint::kFastestInference;
  }
  auto start = base::TimeTicks::Now();
  auto model_impl = backend_->CreateWithResult(
      std::move(params), base::BindOnce(
                             [](base::TimeTicks start) {
                               base::UmaHistogramMediumTimes(
                                   "OnDeviceModel.LoadModelDuration",
                                   base::TimeTicks::Now() - start);
                             },
                             start));
  if (!model_impl.has_value()) {
    std::move(callback).Run(model_impl.error());
    return;
  }
  models_.insert(std::make_unique<OnDeviceModelMojomImpl>(
      std::move(model_impl.value()), std::move(model),
      base::BindOnce(&OnDeviceModelService::DeleteModel,
                     base::Unretained(this))));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

void OnDeviceModelService::GetCapabilities(ModelFile model_file,
                                           GetCapabilitiesCallback callback) {
  std::move(callback).Run(backend_->GetCapabilities(std::move(model_file)));
}

void OnDeviceModelService::GetDeviceAndPerformanceInfo(
    GetDeviceAndPerformanceInfoCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we explicitly allowlist only Chromebook Plus devices,
  // so skip the benchmark and return a fixed performance profile.
  auto perf_info = on_device_model::mojom::DevicePerformanceInfo::New();
  // Fix the performance to 'High', which should allow all Nano models to run.
  perf_info->performance_class =
      on_device_model::mojom::PerformanceClass::kHigh;
  // Chromebook+ devices have 8GB RAM+, so half of that can be VRAM.
  perf_info->vram_mb = 4096;
  auto device_info = on_device_model::mojom::DeviceInfo::New();
  std::move(callback).Run(std::move(perf_info), std::move(device_info));
#else
  // This is expected to take awhile in some cases, so run on a background
  // thread to avoid blocking the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](OnDeviceModelService* service) {
            if (!service) {
              return std::make_pair(
                  on_device_model::mojom::DevicePerformanceInfo::New(),
                  on_device_model::mojom::DeviceInfo::New());
            }
            base::ElapsedTimer timer;
            auto info_pair = service->backend_->GetDeviceAndPerformanceInfo();
            base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration",
                                    timer.Elapsed());
            return info_pair;
          },
          // WeakPtr won't work here because they're not thread-safe.
          // Raw pointers are ok because OnDeviceModelService will always live
          // as long as the ODML process does, so if this code is running the
          // service must be alive.
          base::Unretained(this)),
      base::BindOnce(
          [](GetDeviceAndPerformanceInfoCallback callback,
             std::pair<on_device_model::mojom::DevicePerformanceInfoPtr,
                       on_device_model::mojom::DeviceInfoPtr> info_pair) {
            std::move(callback).Run(std::move(info_pair.first),
                                    std::move(info_pair.second));
          },
          std::move(callback)));
#endif
}

void OnDeviceModelService::LoadTextSafetyModel(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  backend_->LoadTextSafetyModel(std::move(params), std::move(model));
}

void OnDeviceModelService::SetForceQueueingForTesting(bool force_queueing) {
  for (auto& model : models_) {
    static_cast<OnDeviceModelMojomImpl*>(model.get())
        ->SetForceQueueingForTesting(force_queueing);  // IN-TEST
  }
}

void OnDeviceModelService::DeleteModel(
    base::WeakPtr<mojom::OnDeviceModel> model) {
  if (!model) {
    return;
  }
  auto it = models_.find(model.get());
  CHECK(it != models_.end());
  models_.erase(it);
}

}  // namespace on_device_model
