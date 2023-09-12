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

#include <cstdint>
#include <algorithm>
#include <zephyr/sys/mem_blocks.h>

#include "pw_allocator/allocator.h"

namespace pw::allocator::zephyr {

class ZephyrAllocator :
    public pw::allocator::Allocator {
 public:
  ZephyrAllocator(struct sys_mem_blocks *pool, uint8_t alignment)
      : pool_(pool), alignment_(std::max<uint8_t>(1, alignment)) {
  }

  void *Allocate(size_t size) override {
    const auto block_count = ComputeRequiredBlocks(size);

    void *out;
    if (sys_mem_blocks_alloc_contiguous(pool_, block_count, &out) != 0) {
      return nullptr;
    }

    // Save the block count at the head of the memory
    static_cast<uint8_t *>(out)[0] = block_count;

    // Shift 'out' by 1 alignment unit
    return static_cast<uint8_t *>(out) + alignment_;
  }

  void Free(void *ptr) override {
    auto start = static_cast<uint8_t *>(ptr) - alignmnent_;
    sys_mem_blocks_free_contiguous(pool_, start, start[0]);
  }

  void *Realloc(void *ptr, size_t size) override {
    auto start = static_cast<uint8_t *>(ptr) - alignmnent_;
    const auto block_count = start[0];
    const auto new_block_count = ComputeRequiredBlocks(size);

    if (new_block_count == block_count) {
      // Nothing to do
      return ptr;
    }
    if (new_block_count < block_count) {
      // Sizing down, free the tail
      sys_mem_blocks_free_contiguous(pool_,
                                     start + (new_block_count << pool_->blk_sz_shift),
                                     block_count - new_block_count);
      start[0] = new_block_count;
      return ptr;
    }

    // Sizing up, see if we can grow
    if (!sys_mem_blocks_is_region_free(pool_,
                                       start + (block_count << pool_->blk_sz_shift),
                                       new_block_count - block_count)) {
      // Region isn't free, can't grow
      return nullptr;
    }
    if (sys_mem_blocks_get(pool_, start + (block_count << pool_->blk_sz_shift),
                           new_block_count - block_count) != 0) {
      // Failed to allocate the extra blocks
      return nullptr;
    }
    start[0] = new_block_count;
    return ptr;
  }

  void *Calloc(size_t num, size_t size) override {
    return Allocate(num * size);
  }

 private:
  inline uint8_t ComputeRequiredBlocks(size_t size) {
    auto required_size = size + alignment_;
    auto block_count = required_size >> pool_->blk_sz_shift;

    if (required_size - block_count << pool_->blk_sz_shift > 0) {
      block_count++;
    }
    return std::clamp<size_t>(block_count, 1, UINT8_MAX);
  }
  struct sys_mem_blocks *pool_;
  const uint8_t alignment_;
};

}  // pw::allocator::zephyr
