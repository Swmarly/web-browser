// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/platform_dns_query_executor_android.h"

#include <android/multinetwork.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in6.h>
#include <sys/socket.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/task/current_thread.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_names_util.h"

namespace net {

namespace {

// TODO(https://crbug.com/449966580): This is a temporary throwaway parsing
// solution inspired by NativeDnsAsyncTest. Replace it with proper parsing.
constexpr int MAXPACKET = 8 * 1024;

// TODO(https://crbug.com/452586797): Verify this conversion logic is correct.
net_handle_t MapNetworkHandle(handles::NetworkHandle network) {
  if (network == handles::kInvalidNetworkHandle) {
    return NETWORK_UNSPECIFIED;
  }
  return static_cast<net_handle_t>(network);
}

// TODO(https://crbug.com/449966580): This is a temporary throwaway parsing
// solution inspired by NativeDnsAsyncTest. Replace it with proper parsing.
std::vector<std::string> ExtractIpAddressAnswers(base::span<const uint8_t> buf,
                                                 int address_family) {
  ns_msg handle;
  if (ns_initparse(buf.data(), buf.size(), &handle) != 0) {
    return {};
  }
  const int ancount = ns_msg_count(handle, ns_s_an);
  std::vector<std::string> answers;
  for (int i = 0; i < ancount; ++i) {
    ns_rr rr;
    if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) {
      continue;
    }
    std::array<char, INET6_ADDRSTRLEN> buffer;
    if (inet_ntop(address_family, rr.rdata, buffer.data(), buffer.size())) {
      answers.push_back(buffer.data());
    }
  }
  return answers;
}

}  // namespace

PlatformDnsQueryExecutorAndroid::PlatformDnsQueryExecutorAndroid(
    std::string hostname,
    handles::NetworkHandle target_network)
    : hostname_(std::move(hostname)),
      target_network_(target_network),
      read_fd_watcher_(FROM_HERE) {
  // `hostname` must be a valid domain name, and it's the caller's
  // responsibility to check it before calling this constructor.
  DCHECK(dns_names_util::IsValidDnsName(hostname_))
      << "Invalid hostname: " << hostname_;
}

PlatformDnsQueryExecutorAndroid::~PlatformDnsQueryExecutorAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PlatformDnsQueryExecutorAndroid::Start(ResultCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(result_callback);
  CHECK(!result_callback_);
  result_callback_ = std::move(result_callback);

  int fd = android_res_nquery(MapNetworkHandle(target_network_),
                              hostname_.c_str(), ns_c_in, ns_t_a, 0);
  if (fd < 0) {
    OnLookupComplete(net::AddressList(), /*os_error=*/-fd,
                     /*net_error=*/MapSystemError(-fd));
    return;
  }

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          fd, /*persistent=*/false, base::MessagePumpForIO::WATCH_READ,
          &read_fd_watcher_, this)) {
    OnLookupComplete(net::AddressList(), /*os_error=*/0,
                     /*net_error=*/ERR_NAME_NOT_RESOLVED);
    return;
  }
}

void PlatformDnsQueryExecutorAndroid::OnFileCanReadWithoutBlocking(int fd) {
  // TODO(https://crbug.com/450545129): Investigate why this happens.
  // This line is important to keep to avoid internal `MessagePumpEpoll` crash.
  read_fd_watcher_.StopWatchingFileDescriptor();

  ReadResponse(fd);
}

void PlatformDnsQueryExecutorAndroid::ReadResponse(int fd) {
  int rcode = -1;
  std::vector<uint8_t> answer_buf(MAXPACKET);
  int rv =
      android_res_nresult(fd, &rcode, answer_buf.data(), answer_buf.size());

  if (rv < 0) {
    OnLookupComplete(net::AddressList(), /*os_error=*/-rv,
                     /*net_error=*/MapSystemError(-rv));
    return;
  }

  if (rcode != ns_r_noerror) {
    // TODO(https://crbug.com/451557941): Map `rcode` to `net_error`. See the
    // library's mapping.
    OnLookupComplete(net::AddressList(), /*os_error=*/0,
                     /*net_error=*/ERR_NAME_NOT_RESOLVED);
    return;
  }

  AddressList addr_list;
  for (const auto& answer : ExtractIpAddressAnswers(
           base::span(answer_buf).first(static_cast<size_t>(rv)), AF_INET)) {
    const auto ip_address = IPAddress::FromIPLiteral(answer);
    CHECK(ip_address.has_value())
        << "android_res_nresult returned invalid IP address.";
    addr_list.push_back(IPEndPoint(*ip_address, 0));
  }
  OnLookupComplete(addr_list, /*os_error=*/0, /*net_error=*/OK);
}

void PlatformDnsQueryExecutorAndroid::OnLookupComplete(
    const AddressList& addr_list,
    int os_error,
    int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActive());

  // If results are empty, we should return an error.
  if (net_error == OK && addr_list.empty()) {
    net_error = ERR_NAME_NOT_RESOLVED;
  }

  // This class mimics the `HostResolverSystemTask` API, and this logic is
  // copied from there. `net_error` is part of the API because it's returned to
  // the user in the `result_callback_`.
  if (net_error != OK && NetworkChangeNotifier::IsOffline()) {
    net_error = ERR_INTERNET_DISCONNECTED;
  }

  std::move(result_callback_).Run(addr_list, os_error, net_error);
  // Running `result_callback_` can delete `this`.
}

void PlatformDnsQueryExecutorAndroid::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected write on file descriptor.";
}

}  // namespace net
