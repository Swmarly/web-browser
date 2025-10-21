// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ipc/ipc_message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/param_traits_macros.h"
#include "ipc/param_traits_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {

TEST(IPCMessageTest, BasicMessageTest) {
  int v1 = 10;
  std::string v2("foobar");
  std::u16string v3(u"hello world");

  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(v1);
  m.WriteString(v2);
  m.WriteString16(v3);

  base::PickleIterator iter(m);

  int vi;
  std::string vs;
  std::u16string vs16;

  EXPECT_TRUE(iter.ReadInt(&vi));
  EXPECT_EQ(v1, vi);

  EXPECT_TRUE(iter.ReadString(&vs));
  EXPECT_EQ(v2, vs);

  EXPECT_TRUE(iter.ReadString16(&vs16));
  EXPECT_EQ(v3, vs16);

  // should fail
  EXPECT_FALSE(iter.ReadInt(&vi));
  EXPECT_FALSE(iter.ReadString(&vs));
  EXPECT_FALSE(iter.ReadString16(&vs16));
}

TEST(IPCMessageTest, Value) {
  auto expect_value_equals = [](const base::Value& input) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::WriteParam(&msg, input);

    base::Value output;
    base::PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output)) << input;
    EXPECT_EQ(input, output);
  };

  expect_value_equals(base::Value("foo"));
  expect_value_equals(base::Value(42));
  expect_value_equals(base::Value(0.07));
  expect_value_equals(base::Value(true));
  expect_value_equals(base::Value(base::Value::BlobStorage({'a', 'b', 'c'})));

  {
    base::Value::Dict dict;
    dict.Set("key1", 42);
    dict.Set("key2", "hi");
    expect_value_equals(base::Value(std::move(dict)));
  }
  {
    base::Value::List list;
    list.Append(42);
    list.Append("hello");
    expect_value_equals(base::Value(std::move(list)));
  }

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  base::PickleIterator iter(bad_msg);
  base::Value output;
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, ValueDict) {
  base::Value::Dict input;
  input.Set("null", base::Value());
  input.Set("bool", true);
  input.Set("int", 42);
  input.Set("int.with.dot", 43);

  base::Value::Dict subdict;
  subdict.Set("str", "forty two");
  subdict.Set("bool", false);

  base::Value::List sublist;
  sublist.Append(42.42);
  sublist.Append("forty");
  sublist.Append("two");
  subdict.Set("list", std::move(sublist));

  input.Set("dict", std::move(subdict));

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  base::Value::Dict output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_EQ(input, output);

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, FindNext) {
  IPC::Message message;
  message.WriteString("Goooooooogle");
  message.WriteInt(111);

  std::vector<char> message_data(message.size() + 7);
  memcpy(message_data.data(), message.data(), message.size());

  const char* data_start = message_data.data();
  const char* data_end = data_start + message.size();

  IPC::Message::NextMessageInfo next;

  // Data range contains the entire message plus some extra bytes
  IPC::Message::FindNext(data_start, data_end + 1, &next);
  EXPECT_TRUE(next.message_found);
  EXPECT_EQ(next.message_size, message.size());
  EXPECT_EQ(next.pickle_end, data_end);
  EXPECT_EQ(next.message_end, data_end);

  // Data range exactly contains the entire message
  IPC::Message::FindNext(data_start, data_end, &next);
  EXPECT_TRUE(next.message_found);
  EXPECT_EQ(next.message_size, message.size());
  EXPECT_EQ(next.pickle_end, data_end);
  EXPECT_EQ(next.message_end, data_end);

  // Data range doesn't contain the entire message
  // (but contains the message header)
  IPC::Message::FindNext(data_start, data_end - 1, &next);
  EXPECT_FALSE(next.message_found);
  EXPECT_EQ(next.message_size, message.size());

  // Data range doesn't contain the message header
  // (but contains the pickle header)
  IPC::Message::FindNext(data_start,
                         data_start + sizeof(IPC::Message::Header) - 1,
                         &next);
  EXPECT_FALSE(next.message_found);
  EXPECT_EQ(next.message_size, 0u);

  // Data range doesn't contain the pickle header
  IPC::Message::FindNext(data_start,
                         data_start + sizeof(base::Pickle::Header) - 1,
                         &next);
  EXPECT_FALSE(next.message_found);
  EXPECT_EQ(next.message_size, 0u);
}

TEST(IPCMessageTest, FindNextOverflow) {
  IPC::Message message;
  message.WriteString("Data");
  message.WriteInt(777);

  const char* data_start = reinterpret_cast<const char*>(message.data());
  const char* data_end = data_start + message.size();

  IPC::Message::NextMessageInfo next;

  // Payload size is negative (defeats 'start + size > end' check)
  message.header()->payload_size = static_cast<uint32_t>(-1);
  IPC::Message::FindNext(data_start, data_end, &next);
  EXPECT_FALSE(next.message_found);
  if (sizeof(size_t) > sizeof(uint32_t)) {
    // No overflow, just insane message size
    EXPECT_EQ(next.message_size,
              message.header()->payload_size + sizeof(IPC::Message::Header));
  } else {
    // Actual overflow, reported as max size_t
    EXPECT_EQ(next.message_size, std::numeric_limits<size_t>::max());
  }

  // Payload size is max positive integer (defeats size < 0 check, while
  // still potentially causing overflow down the road).
  message.header()->payload_size = std::numeric_limits<int32_t>::max();
  IPC::Message::FindNext(data_start, data_end, &next);
  EXPECT_FALSE(next.message_found);
  EXPECT_EQ(next.message_size,
            message.header()->payload_size + sizeof(IPC::Message::Header));
}

TEST(IPCMessageIntegrity, ReadBeyondBufferStr) {
  // This was BUG 984408.
  uint32_t v1 = std::numeric_limits<uint32_t>::max() - 1;
  int v2 = 666;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(v1);
  m.WriteInt(v2);

  base::PickleIterator iter(m);
  std::string vs;
  EXPECT_FALSE(iter.ReadString(&vs));
}

TEST(IPCMessageIntegrity, ReadBeyondBufferStr16) {
  // This was BUG 984408.
  uint32_t v1 = std::numeric_limits<uint32_t>::max() - 1;
  int v2 = 777;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(v1);
  m.WriteInt(v2);

  base::PickleIterator iter(m);
  std::u16string vs;
  EXPECT_FALSE(iter.ReadString16(&vs));
}

TEST(IPCMessageIntegrity, ReadBytesBadIterator) {
  // This was BUG 1035467.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(1);
  m.WriteInt(2);

  base::PickleIterator iter(m);
  const char* data = nullptr;
  EXPECT_TRUE(iter.ReadBytes(&data, sizeof(int)));
}

TEST(IPCMessageIntegrity, ReadVectorNegativeSize) {
  // A slight variation of BUG 984408. Note that the pickling of vector<char>
  // has a specialized template which is not vulnerable to this bug. So here
  // try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(-1);  // This is the count of elements.
  m.WriteInt(1);
  m.WriteInt(2);
  m.WriteInt(3);

  std::vector<double> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ReadVectorTooLarge1 DISABLED_ReadVectorTooLarge1
#else
#define MAYBE_ReadVectorTooLarge1 ReadVectorTooLarge1
#endif
TEST(IPCMessageIntegrity, MAYBE_ReadVectorTooLarge1) {
  // This was BUG 1006367. This is the large but positive length case. Again
  // we try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(0x21000003);  // This is the count of elements.
  m.WriteInt64(1);
  m.WriteInt64(2);

  std::vector<int64_t> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

TEST(IPCMessageIntegrity, ReadVectorTooLarge2) {
  // This was BUG 1006367. This is the large but positive with an additional
  // integer overflow when computing the actual byte size. Again we try to hit
  // the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(0x71000000);  // This is the count of elements.
  m.WriteInt64(1);
  m.WriteInt64(2);

  std::vector<int64_t> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

// This test needs ~20 seconds in Debug mode, or ~4 seconds in Release mode.
// See http://crbug.com/741866 for details.
TEST(IPCMessageIntegrity, DISABLED_ReadVectorTooLarge3) {
  base::Pickle pickle;
  IPC::WriteParam(&pickle, 256 * 1024 * 1024);
  IPC::WriteParam(&pickle, 0);
  IPC::WriteParam(&pickle, 1);
  IPC::WriteParam(&pickle, 2);

  base::PickleIterator iter(pickle);
  std::vector<int> vec;
  EXPECT_FALSE(IPC::ReadParam(&pickle, &iter, &vec));
}

}  // namespace IPC
