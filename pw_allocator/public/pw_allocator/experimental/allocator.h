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

#include <cstddef>

#include "pw_span/span.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"

namespace pw::allocator::experimental {

class Block;

class Allocator {
 public:
  virtual ~Allocator() {}
  virtual pw::Result<Block> Allocate(size_t size) = 0;
  virtual void Free(Block &data) = 0;
};

class Block {
 public:
  Block(Allocator *allocator, ByteSpan data) : allocator_(allocator), data_(data) {}
  Block(const Block &other) = delete;
  Block(Block &&other) : allocator_(other.allocator_), data_(other.data_) {
    other.allocator_ = nullptr;
    other.data_ = ByteSpan();
  }
  ~Block() {
    if (allocator_ != nullptr) {
      allocator_->Free(*this);
      data_ = ByteSpan();
    }
  }
  Block& operator=(const Block& other) = delete;
  Block& operator=(Block&& other) {
    allocator_ = other.allocator_;
    data_ = other.data_;
    other.allocator_ = nullptr;
    other.data_ = ByteSpan();
    return *this;
  }

  pw::ByteSpan as_byte_span() { return data_; }
  pw::ConstByteSpan as_byte_span() const { return data_; }
  std::byte* data() { return data_.data(); }
  const std::byte* data() const  { return data_.data(); }
  size_t size() const { return data_.size(); }
 private:
  Allocator *allocator_;
  ByteSpan data_;
};

}  // namespace pw::allocator
