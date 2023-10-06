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

#include "pw_allocator/allocator.h"

namespace pw::allocator::zephyr {

class ZephyrAllocator : public pw::allocator::Allocator {
 public:
  ZephyrAllocator() = delete;
  ZephyrAllocator(struct sys_mem_blocks* pool) : pool_(pool) {}
  void* DoAllocate(size_t size, size_t) override {
    const auto block_count = ComputeRequiredBlocks(size);
    void* out;
    if (sys_mem_blocks_alloc_contiguous(pool_, block_count, &out) != 0) {
      return nullptr;
    }
    return out;
  }
  void DoDeallocate(void* ptr, size_t size, size_t) override {
    const auto block_count = ComputeRequiredBlocks(size);
    sys_mem_blocks_free_contiguous(pool_, ptr, block_count);
  }
  bool DoResize(void* ptr,
                size_t old_size,
                size_t,
                size_t new_size) override {
    const auto old_block_count = ComputeRequiredBlocks(old_size);
    const auto new_block_count = ComputeRequiredBlocks(new_size);

    if (new_block_count == old_block_count) {
      return true;
    }
    if (new_block_count < old_block_count) {
      // Can free some space
      const auto block_count = old_block_count - new_block_count;
      ptr = static_cast<uint8_t*>(ptr) +
            (new_block_count << pool_->info.blk_sz_shift);
      return sys_mem_blocks_free_contiguous(pool_, ptr, block_count) == 0;
    }
    // Need to allocate at the tail
    const auto block_count = new_block_count - old_block_count;
    ptr = static_cast<uint8_t*>(ptr) +
          (old_block_count << pool_->info.blk_sz_shift);
    if (!sys_mem_blocks_is_region_free(pool_, ptr, block_count)) {
      return false;
    }

    return sys_mem_blocks_get(pool_, ptr, block_count) == 0;
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
};

}  // namespace pw::allocator::zephyr
