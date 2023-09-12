// Copyright 2023 The Pigweed Authors
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

#include "pw_async_zephyr/rtio_dispatcher.h"

void pw::async::RtioDispatcher::Run() {
  lock_.lock();
  while (!stop_requested_) {
    struct rtio_cqe* cqe = rtio_cqe_consume_block(r_);

    auto rc = cqe->result;
    auto task = static_cast<Task*>(cqe->userdata);
    uint8_t* buffer = nullptr;
    uint32_t buffer_length = 0;

    Context ctx{this, task};
    if (rc != 0) {
      rtio_cqe_release(r_, cqe);
      (*task)(ctx, pw::Status::Internal());
      continue;
    }
    if (rtio_cqe_get_mempool_buffer(r_, cqe, &buffer, &buffer_length) != 0) {
      rtio_cqe_release(r_, cqe);
      (*task)(ctx, pw::Status::ResourceExhausted());
      continue;
    }
    rtio_cqe_release(r_, cqe);
    auto mem_block_result = allocator_.Claim(buffer, buffer_length);
    if (!mem_block_result.ok()) {
      (*task)(ctx, pw::Status::Internal());
      rtio_block_pool_free(r_, buffer, buffer_length);
      continue;
    }

    // TODO add mem_block_result.value to ctx
    (*task)(ctx, pw::OkStatus());
  }
  lock_.unlock();
}
