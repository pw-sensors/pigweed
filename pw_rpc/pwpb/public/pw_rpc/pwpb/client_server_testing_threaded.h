// Copyright 2022 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cinttypes>

#include "pw_rpc/internal/client_server_testing.h"
#include "pw_rpc/internal/client_server_testing_threaded.h"
#include "pw_rpc/pwpb/fake_channel_output.h"

namespace pw::rpc {
namespace internal {

template <size_t kOutputSize,
          size_t kMaxPackets,
          size_t kPayloadsBufferSizeBytes>
class PwpbWatchableChannelOutput final
    : public WatchableChannelOutput<
          PwpbFakeChannelOutput<kMaxPackets, kPayloadsBufferSizeBytes>,
          kOutputSize,
          kMaxPackets,
          kPayloadsBufferSizeBytes> {
 private:
  template <auto kMethod>
  using MethodInfo = internal::MethodInfo<kMethod>;
  template <auto kMethod>
  using Response = typename MethodInfo<kMethod>::Response;
  template <auto kMethod>
  using Request = typename MethodInfo<kMethod>::Request;

  using Base = WatchableChannelOutput<
      PwpbFakeChannelOutput<kMaxPackets, kPayloadsBufferSizeBytes>,
      kOutputSize,
      kMaxPackets,
      kPayloadsBufferSizeBytes>;

 public:
  explicit PwpbWatchableChannelOutput(
      TestPacketProcessor&& server_packet_processor = nullptr,
      TestPacketProcessor&& client_packet_processor = nullptr)
      : Base(std::move(server_packet_processor),
             std::move(client_packet_processor)) {}

  template <auto kMethod>
  Response<kMethod> response(uint32_t channel_id, uint32_t index)
      PW_LOCKS_EXCLUDED(Base::mutex_) {
    std::lock_guard lock(Base::mutex_);
    PW_ASSERT(Base::PacketCount() >= index);
    return Base::output_.template responses<kMethod>(channel_id)[index];
  }

  template <auto kMethod>
  void response(uint32_t channel_id,
                uint32_t index,
                Response<kMethod>& response) PW_LOCKS_EXCLUDED(Base::mutex_) {
    std::lock_guard lock(Base::mutex_);
    PW_ASSERT(Base::PacketCount() >= index);
    auto payloads_view = Base::output_.template responses<kMethod>(channel_id);
    PW_ASSERT(payloads_view.serde()
                  .Decode(payloads_view.payloads()[index], response)
                  .ok());
  }

  template <auto kMethod>
  Request<kMethod> request(uint32_t channel_id, uint32_t index)
      PW_LOCKS_EXCLUDED(Base::mutex_) {
    std::lock_guard lock(Base::mutex_);
    PW_ASSERT(Base::PacketCount() >= index);
    return Base::output_.template requests<kMethod>(channel_id)[index];
  }
};

}  // namespace internal

template <size_t kOutputSize = 128,
          size_t kMaxPackets = 16,
          size_t kPayloadsBufferSizeBytes = 128>
class PwpbClientServerTestContextThreaded final
    : public internal::ClientServerTestContextThreaded<
          internal::PwpbWatchableChannelOutput<kOutputSize,
                                               kMaxPackets,
                                               kPayloadsBufferSizeBytes>,
          kOutputSize,
          kMaxPackets,
          kPayloadsBufferSizeBytes> {
 private:
  template <auto kMethod>
  using MethodInfo = internal::MethodInfo<kMethod>;
  template <auto kMethod>
  using Response = typename MethodInfo<kMethod>::Response;
  template <auto kMethod>
  using Request = typename MethodInfo<kMethod>::Request;

  using Base = internal::ClientServerTestContextThreaded<
      internal::PwpbWatchableChannelOutput<kOutputSize,
                                           kMaxPackets,
                                           kPayloadsBufferSizeBytes>,
      kOutputSize,
      kMaxPackets,
      kPayloadsBufferSizeBytes>;

 public:
  PwpbClientServerTestContextThreaded(
      const thread::Options& options,
      TestPacketProcessor&& server_packet_processor = nullptr,
      TestPacketProcessor&& client_packet_processor = nullptr)
      : Base(options,
             std::move(server_packet_processor),
             std::move(client_packet_processor)) {}

  // Retrieve copy of request indexed by order of occurance
  template <auto kMethod>
  Request<kMethod> request(uint32_t index) {
    return Base::channel_output_.template request<kMethod>(Base::channel().id(),
                                                           index);
  }

  // Retrieve copy of resonse indexed by order of occurance
  template <auto kMethod>
  Response<kMethod> response(uint32_t index) {
    return Base::channel_output_.template response<kMethod>(
        Base::channel().id(), index);
  }

  // Gives access to the RPC's indexed by order of occurance using passed
  // Response object to parse using pw_protobuf. Use this version when you need
  // to set callback fields in the Response object before parsing.
  template <auto kMethod>
  void response(uint32_t index, Response<kMethod>& response) {
    return Base::channel_output_.template response<kMethod>(
        Base::channel().id(), index, response);
  }
};

}  // namespace pw::rpc
