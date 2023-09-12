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

#pragma once

#include <zephyr/sys/mem_blocks.h>

#include <algorithm>
#include <cstdint>

#include "pw_allocator/experimental/allocator.h"
#include "pw_allocator/allocator.h"

namespace pw::allocator::zephyr {

class ZephyrAllocator2 : public pw::allocator::Allocator {
  void* DoAllocate(size_t size, size_t alignment) override { return nullptr; }
  void DoDeallocate(void* ptr, size_t size, size_t alignment) override {}
  bool DoResize(void* ptr,
                size_t old_size,
                size_t old_alignment,
                size_t new_size) override {
    return false;
  }
};

class ZephyrAllocator : public pw::allocator::experimental::Allocator {
 public:
  ZephyrAllocator() = delete;
  ZephyrAllocator(struct sys_mem_blocks* pool, uint8_t alignment)
      : pool_(pool), alignment_(std::max<uint8_t>(1, alignment)) {}

  pw::Result<pw::allocator::experimental::Block> Allocate(
      size_t size) override {
    const auto block_count = ComputeRequiredBlocks(size);

    void* out;
    if (sys_mem_blocks_alloc_contiguous(pool_, block_count, &out) != 0) {
      return {pw::Status::ResourceExhausted()};
    }

    return {std::move(pw::allocator::experimental::Block(
        this,
        ByteSpan(static_cast<std::byte*>(out),
                 block_count << pool_->info.blk_sz_shift)))};
  }

  void Free(pw::allocator::experimental::Block& data) override {
    sys_mem_blocks_free_contiguous(
        pool_, data.data(), data.size() >> pool_->info.blk_sz_shift);
  }

  pw::Result<pw::allocator::experimental::Block> Claim(void* ptr, size_t size) {
    const auto block_count = ComputeRequiredBlocks(size);

    if (sys_mem_blocks_is_region_free(pool_, ptr, block_count)) {
      printk("region is free, can't claim\n");
      return {pw::Status::Unavailable()};
    }
    return {pw::allocator::experimental::Block(
        this,
        ByteSpan(static_cast<std::byte*>(ptr),
                 block_count << pool_->info.blk_sz_shift))};
  }

 private:
  inline size_t ComputeRequiredBlocks(size_t size) {
    auto block_count = size >> pool_->info.blk_sz_shift;

    if (size - (block_count << pool_->info.blk_sz_shift) > 0) {
      block_count++;
    }
    return std::max<size_t>(block_count, 1);
  }
  struct sys_mem_blocks* pool_;
  const uint8_t alignment_;
};

}  // namespace pw::allocator::zephyr
