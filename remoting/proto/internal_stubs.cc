// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/internal_stubs.h"

#include <memory>
#include <string>

#include "base/notreached.h"

namespace remoting::internal {

namespace {
// Rather than using a static string per function or a member in each aliased
// proto, we just use the same empty string here and return it as a const& so
// callers can't modify or mess with it.
const std::string& GetEmptyStringRef() {
  static std::string empty;
  return empty;
}
}  // namespace

// ===========================
// RemoteAccessService helpers
// ===========================

// ProvisionCorpMachine

std::string GetMachineProvisioningRequestPath() {
  return "";
}

std::unique_ptr<ProvisionCorpMachineRequest> GetMachineProvisioningRequest(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    const std::string& version,
    const std::optional<std::string>& existing_directory_id) {
  return std::make_unique<ProvisionCorpMachineRequest>();
}

const std::string& GetAuthorizationCode(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetServiceAccount(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetOwnerEmail(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetHostId(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}

// ReportProvisioningError

std::string GetReportProvisioningErrorRequestPath() {
  return "";
}

std::unique_ptr<ReportProvisioningErrorRequest>
GetReportProvisioningErrorRequest(const std::string& directory_id,
                                  const std::string& error_message,
                                  const std::string& version) {
  return std::make_unique<ReportProvisioningErrorRequest>();
}

// SendHeartbeat

std::string GetSendHeartbeatRequestPath() {
  return "";
}

std::unique_ptr<SendHeartbeatRequest> GetSendHeartbeatRequest(
    const std::string& directory_id) {
  return std::make_unique<SendHeartbeatRequest>();
}

// UpdateRemoteAccessHost

std::string GetUpdateRemoteAccessHostRequestPath() {
  return "";
}

std::unique_ptr<UpdateRemoteAccessHostRequest> GetUpdateRemoteAccessHostRequest(
    const std::string& directory_id,
    std::optional<std::string> host_version,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    std::optional<std::string> os_name,
    std::optional<std::string> os_version) {
  return std::make_unique<UpdateRemoteAccessHostRequest>();
}

// ===========================
// SessionAuthzService helpers
// ===========================

std::string_view GetRemoteAccessSessionAuthzPath() {
  return {};
}

std::string_view GetRemoteSupportSessionAuthzPath() {
  return {};
}

// GenerateHostToken

std::string_view GetGenerateHostTokenRequestVerb() {
  return {};
}

std::unique_ptr<GenerateHostTokenRequest> GetGenerateHostTokenRequest(
    const GenerateHostTokenRequestStruct&) {
  return std::make_unique<GenerateHostTokenRequest>();
}

std::unique_ptr<GenerateHostTokenResponseStruct>
GetGenerateHostTokenResponseStruct(const GenerateHostTokenResponse&) {
  return std::make_unique<GenerateHostTokenResponseStruct>();
}

// VerifySessionToken

std::string_view GetVerifySessionTokenRequestVerb() {
  return {};
}

std::unique_ptr<VerifySessionTokenRequest> GetVerifySessionTokenRequest(
    const VerifySessionTokenRequestStruct&) {
  return std::make_unique<VerifySessionTokenRequest>();
}

std::unique_ptr<VerifySessionTokenResponseStruct>
GetVerifySessionTokenResponseStruct(const VerifySessionTokenResponse&) {
  return std::make_unique<VerifySessionTokenResponseStruct>();
}

// ReauthorizeHost

std::string_view GetReauthorizeHostRequestVerb() {
  return {};
}

std::unique_ptr<ReauthorizeHostRequest> GetReauthorizeHostRequest(
    const ReauthorizeHostRequestStruct&) {
  return std::make_unique<ReauthorizeHostRequest>();
}

extern std::unique_ptr<ReauthorizeHostResponseStruct>
GetReauthorizeHostResponseStruct(const ReauthorizeHostResponse&) {
  return std::make_unique<ReauthorizeHostResponseStruct>();
}

// ======================
// LoggingService helpers
// ======================

std::string_view GetRemoteAccessLoggingPath() {
  return {};
}

std::string_view GetRemoteSupportLoggingPath() {
  return {};
}

// ReportSessionDisconnected

std::string_view GetReportSessionDisconnectedRequestVerb() {
  return {};
}

std::unique_ptr<ReportSessionDisconnectedRequest>
GetReportSessionDisconnectedRequest(
    const ReportSessionDisconnectedRequestStruct&) {
  return std::make_unique<ReportSessionDisconnectedRequest>();
}

// ============================
// RemoteSupportService helpers
// ============================

std::string_view GetCreateRemoteSupportHostRequestPath() {
  return {};
}

std::unique_ptr<RemoteSupportHost> GetRemoteSupportHost(
    const RemoteSupportHostStruct& request_struct) {
  return std::make_unique<RemoteSupportHost>();
}

std::string_view GetSupportId(const RemoteSupportHost&) {
  return {};
}

// ============================
// MessagingService helpers
// ============================

std::string_view GetSendHostMessagePath() {
  return "/fake/for_testing/send_host_message";
}
std::string_view GetReceiveClientMessagesPath() {
  return "/fake/for_testing/receive_client_messages";
}

std::unique_ptr<ReceiveClientMessagesRequest> GetReceiveClientMessagesRequest(
    const ReceiveClientMessagesRequestStruct&) {
  return std::make_unique<ReceiveClientMessagesRequest>();
}

std::unique_ptr<ReceiveClientMessagesResponseStruct>
GetReceiveClientMessagesResponseStruct(const ReceiveClientMessagesResponse&) {
  return std::make_unique<ReceiveClientMessagesResponseStruct>();
}

std::unique_ptr<SendHostMessageRequest> GetSendHostMessageRequest(
    const SendHostMessageRequestStruct&) {
  return std::make_unique<SendHostMessageRequest>();
}

std::unique_ptr<SendHostMessageResponseStruct> GetSendHostMessageResponseStruct(
    const SendHostMessageResponse&) {
  return std::make_unique<SendHostMessageResponseStruct>();
}

std::unique_ptr<SimpleMessage> GetSimpleMessage(const SimpleMessageStruct&) {
  return std::make_unique<SimpleMessage>();
}

std::unique_ptr<SimpleMessageStruct> GetSimpleMessageStruct(
    const SimpleMessage&) {
  return std::make_unique<SimpleMessageStruct>();
}

}  // namespace remoting::internal
