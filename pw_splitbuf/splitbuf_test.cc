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

#include "pw_splitbuf/splitbuf.h"

#include "gtest/gtest.h"
#include "pw_assert/check.h"

namespace pw::splitbuf {
namespace {

TEST(ChunkPool, GetContiguousBufferSmallAmountReturnsMatchSizedChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  const size_t SIZE = 64;
  Result<Chunk> chunk = pool.GetContiguousBuffer(SIZE);
  PW_CHECK_OK(chunk.status());
  PW_CHECK_UINT_EQ(chunk->size(), SIZE);
}

TEST(ChunkPool, GetContiguousBufferWholeSizeFails) {
  constexpr size_t SIZE = 1024;
  std::array<std::byte, SIZE> memory;
  ChunkPool pool(memory);
  PW_CHECK(!pool.GetContiguousBuffer(SIZE).ok());
}

TEST(Chunk, ReleaseReturnsMemoryToPool) {
  constexpr size_t SIZE = 1024;
  std::array<std::byte, SIZE> memory;
  ChunkPool pool(memory);

  Result<Chunk> chunk = pool.GetContiguousBuffer(SIZE / 2);
  PW_CHECK_OK(chunk.status());

  // Allocating a chunk of the same size again initially fails,
  // but succeeds after the first chunk is free'd.
  PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  chunk->Release();
  PW_CHECK_OK(pool.GetContiguousBuffer(SIZE / 2).status());
}

TEST(Chunk, DestructorReturnsMemoryToPool) {
  constexpr size_t SIZE = 1024;
  std::array<std::byte, SIZE> memory;
  ChunkPool pool(memory);

  {
    Result<Chunk> chunk = pool.GetContiguousBuffer(SIZE / 2);
    PW_CHECK_OK(chunk.status());

    // Allocating a chunk of the same size again initially fails,
    // but succeeds after the first chunk is destroyed.
    PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  }
  PW_CHECK_OK(pool.GetContiguousBuffer(SIZE / 2).status());
}

TEST(Chunk, CloneKeepsMemoryUntilBothReleased) {
  constexpr size_t SIZE = 1024;
  std::array<std::byte, SIZE> memory;
  ChunkPool pool(memory);
  Result<Chunk> chunk = pool.GetContiguousBuffer(SIZE / 2);
  PW_CHECK_OK(chunk.status());

  Chunk chunk_2 = chunk->Clone();
  PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  chunk->Release();
  PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  chunk_2.Release();
  PW_CHECK_OK(pool.GetContiguousBuffer(SIZE / 2).status());
}

TEST(Chunk, AdvanceShiftsSpanForward) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  Result<Chunk> chunk_res = pool.GetContiguousBuffer(8);
  PW_CHECK_OK(chunk_res.status());
  Chunk& chunk = chunk_res.value();

  PW_CHECK_UINT_EQ(chunk.size(), 8);
  for (size_t i = 0; i < chunk.size(); i++) {
    chunk.data()[i] = static_cast<std::byte>(i);
  }

  chunk.Advance(3);

  PW_CHECK_UINT_EQ(chunk.size(), 5);
  for (size_t i = 0; i < chunk.size(); i++) {
    PW_CHECK_UINT_EQ(i + 3, chunk.data()[i]);
  }
}

TEST(Chunk, TruncateCutsSpanEnd) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  Result<Chunk> chunk_res = pool.GetContiguousBuffer(8);
  PW_CHECK_OK(chunk_res.status());
  Chunk& chunk = chunk_res.value();

  PW_CHECK_UINT_EQ(chunk.size(), 8);
  for (size_t i = 0; i < chunk.size(); i++) {
    chunk.data()[i] = static_cast<std::byte>(i);
  }

  chunk.Truncate(3);

  PW_CHECK_UINT_EQ(chunk.size(), 3);
  for (size_t i = 0; i < chunk.size(); i++) {
    PW_CHECK_UINT_EQ(i, chunk.data()[i]);
  }
}

TEST(BufferRef, FromChunkReturnsSameData) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  Result<Chunk> chunk_res = pool.GetContiguousBuffer(2);
  PW_CHECK_OK(chunk_res.status());
  Chunk chunk = std::move(chunk_res.value());
  chunk.data()[0] = static_cast<std::byte>(55);
  chunk.data()[1] = static_cast<std::byte>(88);
  Result<BufferRef> buffer_res = BufferRef::FromChunk(std::move(chunk));
  PW_CHECK_OK(buffer_res.status());
  BufferRef& buffer = buffer_res.value();
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].size(), 2);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[0], 55);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[1], 88);
}

TEST(BufferRef, ClonedBufferRefHoldsOntoMemoryUntilBothReleased) {
  constexpr size_t SIZE = 1024;
  std::array<std::byte, SIZE> memory;
  ChunkPool pool(memory);
  Result<Chunk> chunk = pool.GetContiguousBuffer(SIZE / 2);
  PW_CHECK_OK(chunk.status());

  Result<BufferRef> buffer_res = BufferRef::FromChunk(std::move(chunk.value()));
  PW_CHECK_OK(buffer_res.status());
  Result<BufferRef> buffer_res_2 = buffer_res->Clone();
  PW_CHECK_OK(buffer_res_2.status());

  PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  { BufferRef _dropme = std::move(buffer_res.value()); }
  PW_CHECK(!pool.GetContiguousBuffer(SIZE / 2).ok());
  { BufferRef _dropme = std::move(buffer_res_2.value()); }
  PW_CHECK_OK(pool.GetContiguousBuffer(SIZE / 2).status());
}

BufferRef AssertBufferChunk(ChunkPool& pool, pw::span<int> data) {
  Result<Chunk> chunk_res = pool.GetContiguousBuffer(data.size());
  PW_CHECK_OK(chunk_res.status());
  for (size_t i = 0; i < data.size(); i++) {
    chunk_res->data()[i] = static_cast<std::byte>(data[i]);
  }
  Result<BufferRef> buffer_res =
      BufferRef::FromChunk(std::move(chunk_res.value()));
  PW_CHECK_OK(buffer_res.status());
  return std::move(buffer_res.value());
}

TEST(BufferRef, AppendJoinsBuffers) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array<BufferRef, 4> buffers;
  for (int i = 0; i < 4; i++) {
    std::array data = {i};
    buffers[i] = AssertBufferChunk(pool, data);
  }
  // 0 -> (0 + 1)
  PW_CHECK_OK(buffers[0].Append(std::move(buffers[1])));
  // 2 -> (2 + 3)
  PW_CHECK_OK(buffers[2].Append(std::move(buffers[3])));
  // 0 -> ((0 + 1) + (2 + 3))
  PW_CHECK_OK(buffers[0].Append(std::move(buffers[2])));
  PW_CHECK_UINT_EQ(buffers[0].Chunks().size(), 4);
  // Check that `0` is now a buffer containing four chunks, each
  // containing a single byte indicating the index.
  for (int i = 0; i < 4; i++) {
    PW_CHECK_UINT_EQ(buffers[0].Chunks()[i].data()[0], i);
  }
}

TEST(BufferRef, IndexAt) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {0, 0};
  BufferRef buffer = AssertBufferChunk(pool, data);
  std::array data_2 = {0, 0, 0};
  PW_CHECK_OK(buffer.Append(AssertBufferChunk(pool, data_2)));
  std::array<BufferRef::Index, 8> expected = {
      BufferRef::Index{0, 0},
      BufferRef::Index{0, 1},
      BufferRef::Index{1, 0},
      BufferRef::Index{1, 1},
      BufferRef::Index{1, 2},
      // All indices after the end should return the beginning of the first
      // non-existing chunk.
      BufferRef::Index{2, 0},
      BufferRef::Index{2, 0},
      BufferRef::Index{2, 0}};
  for (size_t i = 0; i < expected.size(); i++) {
    BufferRef::Index index = buffer.IndexAt(i);
    PW_CHECK_UINT_EQ(index.chunk_index, expected[i].chunk_index);
    PW_CHECK_UINT_EQ(index.byte_index, expected[i].byte_index);
  }
}

TEST(BufferRef, AdvanceZeroDoesNothing) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24};
  BufferRef buffer = AssertBufferChunk(pool, data);
  PW_CHECK_OK(buffer.Advance(0));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[0], 24);
}

TEST(BufferRef, AdvanceWithinChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  PW_CHECK_OK(buffer.Advance(1));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[0], 37);
}

TEST(BufferRef, AdvanceAcrossChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  std::array data_2 = {33, 44};
  PW_CHECK_OK(buffer.Append(AssertBufferChunk(pool, data_2)));
  PW_CHECK_OK(buffer.Advance(3));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[0], 44);
}

TEST(BufferRef, AdvancePastEndEmpties) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  PW_CHECK_OK(buffer.Advance(5));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 0);
}

TEST(BufferRef, TruncateZeroEmpties) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  PW_CHECK_OK(buffer.Truncate(0));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 0);
}

TEST(BufferRef, TruncateWithinLastChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  std::array data_2 = {33, 44};
  PW_CHECK_OK(buffer.Append(AssertBufferChunk(pool, data_2)));
  PW_CHECK_OK(buffer.Truncate(3));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 2);
  PW_CHECK_UINT_EQ(buffer.Chunks()[1].size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[1].data()[0], 33);
}

TEST(BufferRef, TruncateRemovingLastChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  std::array data_2 = {33, 44};
  PW_CHECK_OK(buffer.Append(AssertBufferChunk(pool, data_2)));
  PW_CHECK_OK(buffer.Truncate(2));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
}

TEST(BufferRef, TruncateWithinFirstChunk) {
  std::array<std::byte, 1024> memory;
  ChunkPool pool(memory);
  std::array data = {24, 37};
  BufferRef buffer = AssertBufferChunk(pool, data);
  std::array data_2 = {33, 44};
  PW_CHECK_OK(buffer.Append(AssertBufferChunk(pool, data_2)));
  PW_CHECK_OK(buffer.Truncate(1));
  PW_CHECK_UINT_EQ(buffer.Chunks().size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].size(), 1);
  PW_CHECK_UINT_EQ(buffer.Chunks()[0].data()[0], 24);
}

}  // namespace
}  // namespace pw::splitbuf
