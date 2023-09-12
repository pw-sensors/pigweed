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

#include <mutex>
#include "pw_splitbuf/splitbuf.h"

#include "pw_assert/check.h"
#include "pw_status/try.h"
#include "pw_sync/mutex.h"

namespace pw::splitbuf {

namespace {

/// This lock guards access to `ChunkHeader`s.
///
/// A global chunk lock (rather than a per-chunk lock) is used in order to avoid
/// the overhead of per-chunk locks.
///
/// N.B.: targets which support atomics could do away with this by using atomic
/// reference counting.
static pw::sync::Mutex chunk_lock;

}  // namespace

namespace internal {

ChunkList::ChunkList(ChunkList&& other) noexcept : chunks_(other.chunks_) {
  other.chunks_ = pw::span<Chunk>();
}
ChunkList& ChunkList::operator=(ChunkList&& other) noexcept {
  Release();
  chunks_ = other.chunks_;
  other.chunks_ = pw::span<Chunk>();
  return *this;
}

Chunk& ChunkList::operator[](size_t index) { return chunks_[index]; }

const Chunk& ChunkList::operator[](size_t index) const {
  return chunks_[index];
}

Result<ChunkList> ChunkList::Allocate(size_t num_chunks) {
  // FIXME: replace this `new` with a facade or similar to allow not
  // using dynamic allocation.
  Chunk* chunks_ptr = new Chunk[num_chunks];
  if (chunks_ptr) {
    return ChunkList(pw::span<Chunk>(chunks_ptr, num_chunks));
  } else {
    return Status::ResourceExhausted();
  }
}

void ChunkList::Release() {
  if (chunks_.size() != 0) {
    delete[] chunks_.data();
  }
  chunks_ = pw::span<Chunk>();
}

ChunkList::~ChunkList() { Release(); }

}  // namespace internal

Chunk::Chunk(pw::allocator::Allocator *allocator, ByteSpan data, internal::ChunkHeader* header)
    : allocator_(allocator), header_(header), data_(data) {}

Chunk Chunk::Clone() {
  {
    std::lock_guard lock(chunk_lock);
    header_->refcount++;
  }
  return Chunk(allocator_, data_, header_);
}

void Chunk::Release() {
  if (header_ == nullptr) {
    return;
  }
  data_ = ByteSpan();

  std::lock_guard lock(chunk_lock);
  header_->refcount--;
  if (header_->refcount == 0) {
    allocator_->Free(static_cast<void*>(header_));
  }
}

void Chunk::Shrink(pw::ByteSpan new_span) {
  PW_DASSERT(new_span.size() <= data_.size());
  PW_DASSERT(new_span.data() >= data_.data());
  data_ = new_span;
  // FIXME: when replacing FreeListHeapBuffer, consider using an approach
  // that allows downsizing the allocation when `header_->refcount == 1`.
}

Result<BufferRef> BufferRef::FromChunk(Chunk chunk) {
  PW_TRY_ASSIGN(internal::ChunkList chunks, internal::ChunkList::Allocate(1));
  chunks[0] = std::move(chunk);
  return BufferRef(std::move(chunks));
}

Result<BufferRef> BufferRef::Clone() {
  PW_TRY_ASSIGN(internal::ChunkList new_chunks,
                internal::ChunkList::Allocate(chunks_.size()));
  for (size_t i = 0; i < chunks_.size(); i++) {
    new_chunks[i] = chunks_[i].Clone();
  }
  return BufferRef(std::move(new_chunks));
}

Status BufferRef::Append(BufferRef suffix) {
  PW_TRY_ASSIGN(
      internal::ChunkList new_chunks,
      internal::ChunkList::Allocate(chunks_.size() + suffix.chunks_.size()));

  for (size_t i = 0; i < chunks_.size(); i++) {
    new_chunks[i] = std::move(chunks_[i]);
  }
  for (size_t i = 0; i < suffix.chunks_.size(); i++) {
    new_chunks[i + chunks_.size()] = std::move(suffix.chunks_[i]);
  }

  chunks_ = std::move(new_chunks);
  suffix.chunks_.Release();

  return OkStatus();
}

Status BufferRef::Advance(BufferRef::Index new_start) {
  // `chunk_index == chunks_.size()` indicates that the `new_start`
  // position is after the end of the current buffer, so we should
  // zero out our current buffer.
  if (new_start.chunk_index == chunks_.size()) {
    chunks_.Release();
    return pw::OkStatus();
  }

  if (new_start.chunk_index != 0) {
    size_t new_chunk_count = chunks_.size() - new_start.chunk_index;
    PW_TRY_ASSIGN(internal::ChunkList new_chunks,
                  internal::ChunkList::Allocate(new_chunk_count));
    for (size_t i = 0; i < new_chunk_count; i++) {
      new_chunks[i] = std::move(chunks_[i + new_start.chunk_index]);
    }
    chunks_ = std::move(new_chunks);
  }

  if (new_start.byte_index != 0) {
    chunks_[0].Advance(new_start.byte_index);
  }

  return pw::OkStatus();
}

Status BufferRef::Advance(size_t new_start) {
  return Advance(IndexAt(new_start));
}

Status BufferRef::Truncate(Index first_index_to_drop) {
  // `chunk_index == chunks_.size()` indicates that the
  // `first_index_to_drop` position is after the end of the current buffer,
  // so we should not make any changes.
  if (first_index_to_drop.chunk_index == chunks_.size()) {
    return pw::OkStatus();
  }

  size_t new_chunk_count;
  if (first_index_to_drop.byte_index == 0) {
    new_chunk_count = first_index_to_drop.chunk_index;
  } else {
    new_chunk_count = first_index_to_drop.chunk_index + 1;
  }

  if (new_chunk_count != chunks_.size()) {
    PW_TRY_ASSIGN(internal::ChunkList new_chunks,
                  internal::ChunkList::Allocate(new_chunk_count));
    for (size_t i = 0; i < new_chunk_count; i++) {
      new_chunks[i] = std::move(chunks_[i]);
    }
    chunks_ = std::move(new_chunks);
  }

  // If the `byte_index` was zero, we've already dropped the whole
  // chunk.
  if (first_index_to_drop.byte_index == 0) {
    return pw::OkStatus();
  }

  chunks_[chunks_.size() - 1].Truncate(first_index_to_drop.byte_index);

  return pw::OkStatus();
}

Status BufferRef::RemoveSegment(Index cut_start, Index resume_point) {
  size_t first_chunk_to_drop;
  if (cut_start.byte_index == 0) {
    first_chunk_to_drop = cut_start.chunk_index;
  } else {
    first_chunk_to_drop = cut_start.chunk_index + 1;
  }

  size_t old_chunk_count = chunks_.size();
  size_t new_chunk_count;
  if (resume_point.chunk_index <= first_chunk_to_drop) {
    // If no chunks are being dropped, we may actually need a *new* chunk in
    // order to represent both sides of the chunk that has been split in two.
    if (cut_start.byte_index == 0) {
      new_chunk_count = old_chunk_count;
    } else {
      new_chunk_count = old_chunk_count + 1;
    }
  } else {
    size_t chunks_dropped = resume_point.chunk_index - first_chunk_to_drop;
    new_chunk_count = chunks_.size() - chunks_dropped;
  }

  if (new_chunk_count != old_chunk_count) {
    PW_TRY_ASSIGN(internal::ChunkList new_chunks,
                  internal::ChunkList::Allocate(new_chunk_count));
    size_t new_i = 0;
    while (new_i < first_chunk_to_drop) {
      new_chunks[new_i] = std::move(chunks_[new_i]);
      new_i++;
    }
    size_t resume_i = resume_point.chunk_index;
    while (resume_i < chunks_.size()) {
      new_chunks[new_i] = std::move(chunks_[resume_i]);
      new_i++;
      resume_i++;
    }
    chunks_ = std::move(new_chunks);
    resume_point.chunk_index = first_chunk_to_drop;
  }

  if (cut_start.byte_index != 0) {
    chunks_[cut_start.chunk_index].Truncate(cut_start.byte_index);
  }
  if (resume_point.byte_index != 0) {
    chunks_[resume_point.chunk_index].Advance(resume_point.byte_index);
  }

  return pw::OkStatus();
}

Status BufferRef::Truncate(size_t len) { return Truncate(IndexAt(len)); }

BufferRef::Index BufferRef::IndexAt(size_t position) const {
  size_t chunk_start = 0;
  size_t chunk_i = 0;
  for (; chunk_i < chunks_.size(); chunk_i++) {
    size_t current_chunk_size = chunks_[chunk_i].size();
    if (chunk_start + current_chunk_size > position) {
      return Index{chunk_i, position - chunk_start};
    }
    chunk_start += current_chunk_size;
  }
  // The index is past the last element. Return an index pointing to the first
  // element of the first non-extant chunk.
  return Index{chunk_i, 0};
}

template<bool use_headers>
Result<Chunk> ChunkPool::GetContiguousBuffer(size_t min_size,
                                             size_t preferred_size) {
  size_t actual_size;
  void* allocation =
      allocator_->Allocate(preferred_size + sizeof(internal::ChunkHeader));
  if (allocation != nullptr) {
    actual_size = preferred_size;
  } else {
    // FIXME: do something smarter when this is unbundled from `FreeListHeap`.
    allocation = allocator_->Allocate(min_size + sizeof(internal::ChunkHeader));
    if (allocation == nullptr) {
      return pw::Status::ResourceExhausted();
    }
    actual_size = min_size;
  }

  internal::ChunkHeader *header = nullptr;
  if (std::is_enabled<use_headers>) {
    header = static_cast<internal::ChunkHeader*>(allocation);
    header->refcount = 1;
  }
  auto span = ByteSpan(
      static_cast<std::byte*>(allocation) + (header == nullptr ? 0 : sizeof(internal::ChunkHeader)),
      actual_size);
  return Chunk(allocator_, span, header);
}

}  // namespace pw::splitbuf
