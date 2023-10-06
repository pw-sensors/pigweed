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

#include "pw_allocator/allocator.h"
#include "pw_bytes/span.h"

namespace pw::sensor {

class Block {
 public:
  Block(pw::allocator::Allocator* allocator, ByteSpan data)
      : allocator_(allocator), data_(data) {}
  Block(const Block& other) = delete;
  Block(Block&& other) noexcept
      : allocator_(other.allocator_), data_(other.data_) {
    other.allocator_ = nullptr;
    other.data_ = ByteSpan();
  }
  ~Block() {
    if (allocator_ != nullptr) {
      allocator_->DeallocateUnchecked(data(), size(), 4);
      data_ = ByteSpan();
    }
  }
  Block& operator=(const Block& other) = delete;
  Block& operator=(Block&& other) noexcept {
    allocator_ = other.allocator_;
    data_ = other.data_;
    other.allocator_ = nullptr;
    other.data_ = ByteSpan();
    return *this;
  }

  pw::ByteSpan as_byte_span() { return data_; }
  pw::ConstByteSpan as_byte_span() const { return data_; }
  std::byte* data() { return data_.data(); }
  const std::byte* data() const { return data_.data(); }
  size_t size() const { return data_.size(); }

 private:
  pw::allocator::Allocator* allocator_;
  ByteSpan data_;
};

}  // namespace pw::sensor
