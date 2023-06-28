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
#include <cstdint>
#include <iterator>

#include "pw_allocator/freelist_heap.h"
#include "pw_bytes/span.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::splitbuf {

class BufferRef;
class Chunk;

namespace internal {

PW_PACKED(struct) ChunkHeader {
  pw::allocator::FreeListHeapBuffer<>* heap;
  uint8_t refcount;
};

class ChunkList {
 public:
  ChunkList() : chunks_(pw::span<Chunk>()) {}
  static Result<ChunkList> Allocate(size_t num_chunks);
  ChunkList(ChunkList&& other) noexcept;
  ChunkList& operator=(ChunkList&& other) noexcept;
  ~ChunkList();

  Chunk& operator[](size_t index);
  const Chunk& operator[](size_t index) const;
  size_t size() const { return chunks_.size(); }
  pw::span<Chunk> span() { return chunks_; }
  pw::span<const Chunk> span() const { return chunks_; }
  void Release();

 private:
  ChunkList(pw::span<Chunk> chunks) : chunks_(chunks) {}
  pw::span<Chunk> chunks_;
};

}  // namespace internal

/// A handle to a contiguous refcounted slice of data.
///
/// Note: this Chunk may acquire a `pw::sync::Mutex` during destruction, and
/// so must not be destroyed within ISR context.
class Chunk {
 public:
  Chunk() : header_(nullptr), data_() {}
  Chunk(Chunk&& other) noexcept : header_(other.header_), data_(other.data_) {
    other.header_ = nullptr;
    other.data_ = ByteSpan();
  }
  Chunk& operator=(Chunk&& other) {
    Release();
    header_ = other.header_;
    data_ = other.data_;
    other.header_ = nullptr;
    other.data_ = ByteSpan();
    return *this;
  }
  ~Chunk() { Release(); }
  std::byte* data() { return data_.data(); }
  const std::byte* data() const { return data_.data(); }
  ByteSpan span() { return data_; }
  ConstByteSpan span() const { return data_; }
  size_t size() const { return data_.size(); }

  /// Creates a copy of this `Chunk`, incrementing the reference count.
  Chunk Clone();

  /// Decrements the reference count and empties this handle.
  ///
  /// Does not modify the underlying data, but may cause it to be
  /// released if this was the only remaining `Chunk` referring to it.
  /// This is equivalent to `{ Chunk _unused = std::move(chunk); }`
  void Release();

  /// Shifts this handle to refer to the data beginning at offset `new_start`.
  ///
  /// Does not modify the underlying data.
  void Advance(size_t new_start) {
    Shrink(span().last(span().size() - new_start));
  }

  /// Shifts and truncates this handle to refer to data in the range
  /// `start..<stop`.
  ///
  /// Does not modify the underlying data.
  void Slice(size_t start, size_t stop) {
    Shrink(span().subspan(start, stop - start));
  }

  /// Truncates this handle to refer to only the first `len` bytes.
  ///
  /// Does not modify the underlying data.
  void Truncate(size_t len) { Shrink(span().first(len)); }

 private:
  Chunk(internal::ChunkHeader* header_, ByteSpan data);

  /// Resizes the portion of the data that this handle refers to.
  ///
  /// `new_span` *must* be a subspan of `data`.
  /// Does not modify the underlying data.
  void Shrink(pw::ByteSpan new_span);

  friend class ChunkPool;
  internal::ChunkHeader* header_;
  ByteSpan data_;
};

/// A handle to a sequence of potentially non-contiguous refcounted slices of
/// data.
class BufferRef {
 public:
  struct Index {
    size_t chunk_index;
    size_t byte_index;
  };

  BufferRef() = default;

  /// Creates a `BufferRef` pointing to a single, contiguous chunk of data.
  ///
  /// May fail if the `ChunkList` allocator is out of memory.
  static Result<BufferRef> FromChunk(Chunk chunk);
  Result<BufferRef> Clone();

  /// Appends the contents of `suffix` to this `BufferRef` without copying data.
  Status Append(BufferRef suffix);

  /// Discards the first elements of `BufferRef` up to (but not including)
  /// `new_start`.
  Status Advance(Index new_start);
  Status Advance(size_t new_start);

  /// Shifts and truncates this handle to refer to data in the range
  /// `start..<stop`.
  ///
  /// Does not modify the underlying data.
  Status Slice(size_t start, size_t stop) {
    PW_TRY(Advance(start));
    return Truncate(stop - start);
  }

  /// Discards the tail of this `BufferRef` starting with `first_index_to_drop`.
  Status Truncate(Index first_index_to_drop);
  /// Discards the tail of this `BufferRef` so that only `len` elements remain.
  Status Truncate(size_t len);

  /// Discards the elements beginning with `cut_start` and resuming at
  /// `resume_point`.
  Status RemoveSegment(Index cut_start, Index resume_point);

  /// Returns an iterable over the `Chunk`s of memory within this `BufferRef`.
  auto Chunks() { return chunks_.span(); }
  auto Chunks() const { return chunks_.span(); }

  /// Returns an `Index` which can be used to provide constant-time access to
  /// the element at `position`.
  ///
  /// Any mutation of this `BufferRef` (e.g. `Append`, `Advance`, `Slice`, or
  /// `Truncate`) may invalidate this `Index`.
  Index IndexAt(size_t position) const;

 private:
  BufferRef(internal::ChunkList chunks_) : chunks_(std::move(chunks_)) {}
  internal::ChunkList chunks_;
};

class ChunkPool {
 public:
  ChunkPool(ByteSpan memory) : heap_(memory) {}
  Result<Chunk> GetContiguousBuffer(size_t size) {
    return GetContiguousBuffer(size, size);
  }
  Result<Chunk> GetContiguousBuffer(size_t min_size, size_t preferred_size);

  // FIXME: support allocating non-contiguous buffers?
  // Result<BufferRef> GetBuffer(size_t size) { return GetBuffer(size, size); }
  // Result<BufferRef> GetBuffer(size_t min_size, size_t preferred_size);

  // FIXME: unbundle implementation from `freelist_heap` and support
  // asynchronous waits.
  //
  // class ContiguousBufferFuture;
  // ContiguousBufferFuture GetContiguousBufferAsync(size_t size) {
  //   return GetContiguousBufferAsync(size, size);
  // }
  // ContiguousBufferFuture GetContiguousBufferAsync(size_t min_size,
  //
  // class BufferFuture;
  // BufferFuture GetBufferAsync(size_t size) {
  //   return GetBufferAsync(size, size);
  // }
  // BufferFuture GetBufferAsync(size_t min_size, size_t preferred_size);

  // class ContiguousBufferFuture {
  //   ContiguousBufferFuture(ChunkPool& pool);
  //   // Poll<Chunk> Poll(Waker& waker);
  // };

  // class BufferFuture {
  //   BufferFuture(ChunkPool& pool);
  //   // Poll<BufferRef> Poll(Waker& waker);
  // };

 private:
  pw::allocator::FreeListHeapBuffer<> heap_;
};

}  // namespace pw::splitbuf
