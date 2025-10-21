// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/webui/boca_receiver_app_ui/audio_packet_converter.h"
#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom-data-view.h"
#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "ash/webui/boca_receiver_app_ui/url_constants.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/get_receiver_connection_info_request.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/proto/audio.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kRequesterId = "boca-receiver";

std::unique_ptr<boca::BocaRequest::Delegate>
CreateConnectionInfoRequestDelegate(
    const std::string& receiver_id,
    GetReceiverConnectionInfoRequest::ResponseCallback callback) {
  return std::make_unique<GetReceiverConnectionInfoRequest>(
      receiver_id, std::move(callback));
}

std::unique_ptr<boca::BocaRequest::Delegate>
CreateUpdateReceiverStateRequestDelegate(
    const std::string& receiver_id,
    const std::string& connection_id,
    ::boca::ReceiverConnectionState connection_state,
    UpdateKioskReceiverStateRequest::ResponseCallback callback) {
  return std::make_unique<UpdateKioskReceiverStateRequest>(
      receiver_id, connection_id, connection_state, std::move(callback));
}

}  // namespace

BocaReceiverUntrustedPageHandler::BocaReceiverUntrustedPageHandler(
    mojo::PendingRemote<mojom::UntrustedPage> page,
    ReceiverHandlerDelegate* delegate)
    : page_(std::move(page)), delegate_(delegate) {
  if (!delegate_->IsAppEnabled(kChromeBocaReceiverURL)) {
    page_->OnInitReceiverError();
    return;
  }
  Init();
}

BocaReceiverUntrustedPageHandler::~BocaReceiverUntrustedPageHandler() {
  if (invalidation_service_) {
    invalidation_service_->ShutDown();
  }
}

void BocaReceiverUntrustedPageHandler::UploadToken(
    const std::string& fcm_token,
    base::OnceCallback<void(bool)> on_token_uploaded_cb) {
  Register(fcm_token, std::move(on_token_uploaded_cb));
}

void BocaReceiverUntrustedPageHandler::OnInvalidationReceived(
    const std::string& payload) {
  if (!receiver_id_.has_value()) {
    return;
  }
  GetConnectionInfo();
}

void BocaReceiverUntrustedPageHandler::Init() {
  invalidation_service_ = delegate_->CreateInvalidationService(this);
}

std::unique_ptr<google_apis::RequestSender>
BocaReceiverUntrustedPageHandler::SendRequest(
    std::unique_ptr<boca::BocaRequest::Delegate> request_delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto request_sender =
      delegate_->CreateRequestSender(kRequesterId, traffic_annotation);
  auto request = std::make_unique<boca::BocaRequest>(
      request_sender.get(), std::move(request_delegate));
  request_sender->StartRequestWithAuthRetry(std::move(request));
  return request_sender;
}

void BocaReceiverUntrustedPageHandler::Register(
    const std::string& fcm_token,
    base::OnceCallback<void(bool)> on_done_cb) {
  auto response_cb =
      base::BindOnce(&BocaReceiverUntrustedPageHandler::OnRegisterResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_done_cb));
  auto registration_request_delegate =
      std::make_unique<RegisterReceiverRequest>(fcm_token,
                                                std::move(response_cb));
  registration_request_sender_ =
      SendRequest(std::move(registration_request_delegate),
                  RegisterReceiverRequest::kTrafficAnnotation);
}

void BocaReceiverUntrustedPageHandler::OnRegisterResponse(
    base::OnceCallback<void(bool)> on_done_cb,
    std::optional<std::string> receiver_id) {
  if (!receiver_id.has_value()) {
    page_->OnInitReceiverError();
    std::move(on_done_cb).Run(false);
    return;
  }
  mojom::ReceiverInfoPtr receiver_info = mojom::ReceiverInfo::New();
  receiver_info->id = receiver_id.value();
  page_->OnInitReceiverInfo(std::move(receiver_info));
  std::move(on_done_cb).Run(true);
  receiver_id_ = std::move(receiver_id.value());
  GetConnectionInfo();
}

void BocaReceiverUntrustedPageHandler::UpdateConnection(
    const std::string& connection_id,
    ::boca::ReceiverConnectionState request_state) {
  CHECK(receiver_id_.has_value());
  constexpr int kMaxRetries = 3;
  // Use the same `update_connection_retriable_sender_` to avoid cancelling any
  // pending update requests.
  if (!update_connection_retriable_sender_) {
    std::unique_ptr<google_apis::RequestSender> request_sender =
        delegate_->CreateRequestSender(
            kRequesterId, UpdateKioskReceiverStateRequest::kTrafficAnnotation);
    update_connection_retriable_sender_ =
        std::make_unique<UpdateReceiverStateRequestSender>(
            std::move(request_sender), kMaxRetries);
  }
  auto create_request_delegate_cb =
      base::BindRepeating(&CreateUpdateReceiverStateRequestDelegate,
                          receiver_id_.value(), connection_id, request_state);
  auto response_cb = base::BindOnce(
      &BocaReceiverUntrustedPageHandler::OnUpdateConnectionResponse,
      weak_ptr_factory_.GetWeakPtr());
  update_connection_retriable_sender_->SendRequest(
      std::move(create_request_delegate_cb), std::move(response_cb));
}

void BocaReceiverUntrustedPageHandler::OnUpdateConnectionResponse(
    std::optional<::boca::ReceiverConnectionState> response_state) {}

void BocaReceiverUntrustedPageHandler::GetConnectionInfo() {
  CHECK(receiver_id_.has_value());
  constexpr int kMaxRetries = 5;
  std::unique_ptr<google_apis::RequestSender> request_sender =
      delegate_->CreateRequestSender(
          kRequesterId, GetReceiverConnectionInfoRequest::kTrafficAnnotation);
  auto create_request_delegate_cb = base::BindRepeating(
      &CreateConnectionInfoRequestDelegate, receiver_id_.value());
  auto response_cb = base::BindOnce(
      &BocaReceiverUntrustedPageHandler::OnGetConnectionInfoResponse,
      weak_ptr_factory_.GetWeakPtr());
  connection_info_retriable_sender_ =
      std::make_unique<ConnectionInfoRequestSender>(std::move(request_sender),
                                                    kMaxRetries);
  connection_info_retriable_sender_->SendRequest(
      std::move(create_request_delegate_cb), std::move(response_cb));
}

void BocaReceiverUntrustedPageHandler::OnGetConnectionInfoResponse(
    std::optional<::boca::KioskReceiverConnection> new_connection_info) {
  if (!new_connection_info.has_value() ||
      new_connection_info->connection_id().empty()) {
    return;
  }
  const ::boca::ReceiverConnectionState connection_state =
      new_connection_info->receiver_connection_state();
  switch (connection_state) {
    case ::boca::ReceiverConnectionState::START_REQUESTED:
      ProcessStartRequested(std::move(new_connection_info.value()));
      break;
    case ::boca::ReceiverConnectionState::STOP_REQUESTED:
      ProcessStopRequested(new_connection_info.value());
      break;
    case ::boca::ReceiverConnectionState::CONNECTING:
    case ::boca::ReceiverConnectionState::CONNECTED:
      if (!connection_info_.has_value()) {
        // If there is no ongoing connection but the state at the server is
        // CONNECTING or CONNECTED, update the server with DISCONNECTED state.
        // This may happen if the receiver was shutdown or crash in the middle
        // of a session.
        UpdateConnection(new_connection_info->connection_id(),
                         ::boca::ReceiverConnectionState::DISCONNECTED);
      }
      break;
    default:
      break;
  }
}

void BocaReceiverUntrustedPageHandler::ProcessStartRequested(
    ::boca::KioskReceiverConnection new_connection_info) {
  CHECK_EQ(new_connection_info.receiver_connection_state(),
           ::boca::ReceiverConnectionState::START_REQUESTED);
  if (connection_info_.has_value() &&
      connection_info_->connection_id() ==
          new_connection_info.connection_id() &&
      connection_info_->receiver_connection_state() !=
          ::boca::ReceiverConnectionState::START_REQUESTED) {
    LOG(ERROR) << "[BocaReceiver] Unexpected connection info state "
               << new_connection_info.receiver_connection_state()
               << ", current state is: "
               << connection_info_->receiver_connection_state();
    // START_REQUESTED is already processed for this connection.
    return;
  }
  if (connection_info_.has_value() && connection_info_->connection_id() !=
                                          new_connection_info.connection_id()) {
    MaybeEndConnection(mojom::ConnectionClosedReason::kTakeOver);
  }
  MaybeStartConnection(std::move(new_connection_info));
}

void BocaReceiverUntrustedPageHandler::ProcessStopRequested(
    const ::boca::KioskReceiverConnection& new_connection_info) {
  CHECK_EQ(new_connection_info.receiver_connection_state(),
           ::boca::ReceiverConnectionState::STOP_REQUESTED);
  if (!connection_info_.has_value() ||
      connection_info_->connection_id() !=
          new_connection_info.connection_id()) {
    UpdateConnection(new_connection_info.connection_id(),
                     ::boca::ReceiverConnectionState::DISCONNECTED);
    return;
  }
  MaybeEndConnection(mojom::ConnectionClosedReason::kInitiatorClosed);
}

void BocaReceiverUntrustedPageHandler::MaybeStartConnection(
    ::boca::KioskReceiverConnection new_connection_info) {
  connection_info_ = std::move(new_connection_info);
  if (connection_info_->receiver_connection_state() !=
          ::boca::START_REQUESTED ||
      connection_info_->connection_details()
          .connection_code()
          .connection_code()
          .empty()) {
    return;
  }
  const ::boca::UserIdentity& initiator =
      connection_info_->connection_details().initiator().user_identity();
  const ::boca::UserIdentity& presenter =
      connection_info_->connection_details().presenter().user_identity();
  page_->OnConnecting(mojom::UserInfo::New(initiator.full_name()),
                      initiator.gaia_id() != presenter.gaia_id()
                          ? mojom::UserInfo::New(presenter.full_name())
                          : nullptr);
  connection_info_->set_receiver_connection_state(
      ::boca::ReceiverConnectionState::CONNECTING);
  UpdateConnection(connection_info_->connection_id(),
                   ::boca::ReceiverConnectionState::CONNECTING);
  std::string connection_code = connection_info_->connection_details()
                                    .connection_code()
                                    .connection_code();
  remoting_client_ = delegate_->CreateRemotingClientManager();
  remoting_client_->StartCrdClient(
      std::move(connection_code),
      base::BindOnce(&BocaReceiverUntrustedPageHandler::OnCrdSessionEnded,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&BocaReceiverUntrustedPageHandler::OnCrdFrameReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &BocaReceiverUntrustedPageHandler::OnCrdAudioPacketReceived,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &BocaReceiverUntrustedPageHandler::OnCrdConnectionStateUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void BocaReceiverUntrustedPageHandler::MaybeEndConnection(
    mojom::ConnectionClosedReason reason) {
  if (!connection_info_.has_value()) {
    return;
  }
  if (connection_info_->receiver_connection_state() == ::boca::CONNECTED ||
      connection_info_->receiver_connection_state() == ::boca::CONNECTING) {
    CHECK(remoting_client_);
    page_->OnConnectionClosed(reason);
    auto* remoting_client_ptr = remoting_client_.get();
    remoting_client_ptr->StopCrdClient(
        base::BindOnce([](std::unique_ptr<boca::SpotlightRemotingClientManager>
                              remoting_client) { remoting_client.reset(); },
                       std::move(remoting_client_)));
  }
  auto connection_state = reason == mojom::ConnectionClosedReason::kError
                              ? ::boca::ReceiverConnectionState::ERROR
                              : ::boca::ReceiverConnectionState::DISCONNECTED;
  UpdateConnection(connection_info_->connection_id(), connection_state);
  connection_info_.reset();
}

void BocaReceiverUntrustedPageHandler::OnCrdSessionEnded() {
  MaybeEndConnection(mojom::ConnectionClosedReason::kPresenterConnectionLost);
}

void BocaReceiverUntrustedPageHandler::OnCrdFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame>) {
  CHECK(connection_info_.has_value());
  page_->OnFrameReceived(bitmap);
  if (connection_info_->receiver_connection_state() ==
      ::boca::ReceiverConnectionState::CONNECTED) {
    return;
  }
  connection_info_->set_receiver_connection_state(
      ::boca::ReceiverConnectionState::CONNECTED);
  UpdateConnection(connection_info_->connection_id(),
                   ::boca::ReceiverConnectionState::CONNECTED);
}

void BocaReceiverUntrustedPageHandler::OnCrdAudioPacketReceived(
    std::unique_ptr<remoting::AudioPacket> packet) {
  boca_receiver::mojom::DecodedAudioPacketPtr mojom_packet =
      ConvertAudioPacketToMojom(std::move(packet));
  if (mojom_packet) {
    page_->OnAudioPacket(std::move(mojom_packet));
  } else {
    LOG(ERROR) << "Dropping audio packet due to conversion failure.";
  }
}

void BocaReceiverUntrustedPageHandler::OnCrdConnectionStateUpdated(
    boca::CrdConnectionState state) {
  switch (state) {
    case boca::CrdConnectionState::kDisconnected:
    case boca::CrdConnectionState::kTimeout:
      MaybeEndConnection(
          mojom::ConnectionClosedReason::kPresenterConnectionLost);
      break;
    case boca::CrdConnectionState::kFailed:
      MaybeEndConnection(mojom::ConnectionClosedReason::kError);
      break;
    case boca::CrdConnectionState::kUnknown:
    case boca::CrdConnectionState::kConnecting:
    case boca::CrdConnectionState::kConnected:
      break;
  }
}

}  // namespace ash::boca_receiver
