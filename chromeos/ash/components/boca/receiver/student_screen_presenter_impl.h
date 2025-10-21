// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_STUDENT_SCREEN_PRESENTER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_STUDENT_SCREEN_PRESENTER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
#include "chromeos/ash/components/boca/student_screen_presenter.h"

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

class StudentScreenPresenterImpl : public StudentScreenPresenter {
 public:
  StudentScreenPresenterImpl(
      std::string_view session_id,
      const ::boca::UserIdentity& teacher_identity,
      std::string_view teacher_device_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  StudentScreenPresenterImpl(const StudentScreenPresenterImpl&) = delete;
  StudentScreenPresenterImpl& operator=(const StudentScreenPresenterImpl&) =
      delete;

  ~StudentScreenPresenterImpl() override;

  // StudentScreenPresenter:
  void Start(std::string_view receiver_id,
             const ::boca::UserIdentity& student_identity,
             std::string_view student_device_id,
             base::OnceCallback<void(bool)> success_cb,
             base::OnceClosure disconnected_cb) override;
  void CheckConnection() override;
  void Stop(base::OnceCallback<void(bool)> success_cb) override;
  bool IsPresenting(std::optional<std::string_view> student_id) override;

 private:
  void OnStartResponse(base::OnceCallback<void(bool)> success_cb,
                       std::optional<std::string> connection_id);

  void OnCheckConnectionResponse(std::optional<::boca::KioskReceiver> receiver);

  void OnStopResponse(
      std::optional<::boca::ReceiverConnectionState> connection_state);

  void Reset();

  void NotifyStopSuccess(bool success);

  const std::string session_id_;
  const ::boca::UserIdentity teacher_identity_;
  const std::string teacher_device_id_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::optional<std::string> receiver_id_;
  base::OnceClosure disconnected_cb_;
  std::optional<std::string> connection_id_;
  std::optional<std::string> student_id_;
  std::unique_ptr<google_apis::RequestSender> start_connection_request_sender_;
  std::unique_ptr<google_apis::RequestSender> get_receiver_request_sender_;
  std::unique_ptr<google_apis::RequestSender> update_connection_request_sender_;
  bool stop_request_in_progress_ = false;
  base::queue<base::OnceCallback<void(bool)>> stop_success_callbacks_;
  base::OneShotTimer stopped_check_timer_;

  base::WeakPtrFactory<StudentScreenPresenterImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_STUDENT_SCREEN_PRESENTER_IMPL_H_
