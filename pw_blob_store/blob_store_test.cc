// Copyright 2020 The Pigweed Authors
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

#include "pw_blob_store/blob_store.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include "gtest/gtest.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/test_key_value_store.h"
#include "pw_log/log.h"
#include "pw_random/xor_shift.h"

namespace pw::blob_store {
namespace {

class BlobStoreTest : public ::testing::Test {
 protected:
  BlobStoreTest() : flash_(kFlashAlignment), partition_(&flash_) {}

  void InitFlashTo(std::span<const std::byte> contents) {
    partition_.Erase();
    std::memcpy(flash_.buffer().data(), contents.data(), contents.size());
  }

  void InitSourceBufferToRandom(uint64_t seed) {
    partition_.Erase();
    random::XorShiftStarRng64 rng(seed);
    rng.Get(source_buffer_);
  }

  void InitSourceBufferToFill(char fill) {
    partition_.Erase();
    std::memset(source_buffer_.data(), fill, source_buffer_.size());
  }

  // Fill the source buffer with random pattern based on given seed, written to
  // BlobStore in specified chunk size.
  void ChunkWriteTest(size_t chunk_size) {
    constexpr size_t kBufferSize = 256;
    kvs::ChecksumCrc16 checksum;

    char name[16] = {};
    snprintf(name, sizeof(name), "Blob%u", static_cast<unsigned>(chunk_size));

    BlobStoreBuffer<kBufferSize> blob(
        name, &partition_, &checksum, kvs::TestKvs());
    EXPECT_EQ(Status::OK, blob.Init());

    BlobStore::BlobWriter writer(blob);
    EXPECT_EQ(Status::OK, writer.Open());

    ByteSpan source = source_buffer_;
    while (source.size_bytes() > 0) {
      const size_t write_size = std::min(source.size_bytes(), chunk_size);

      PW_LOG_DEBUG("Do write of %u bytes, %u bytes remain",
                   static_cast<unsigned>(write_size),
                   static_cast<unsigned>(source.size_bytes()));

      EXPECT_EQ(Status::OK, writer.Write(source.first(write_size)));

      source = source.subspan(write_size);
    }

    EXPECT_EQ(Status::OK, writer.Close());

    // Use reader to check for valid data.
    BlobStore::BlobReader reader(blob, 0);
    EXPECT_EQ(Status::OK, reader.Open());
    Result<ByteSpan> result = reader.GetMemoryMappedBlob();
    ASSERT_TRUE(result.ok());
    VerifyFlash(result.value());
    EXPECT_EQ(Status::OK, reader.Close());
  }

  void VerifyFlash(ByteSpan verify_bytes) {
    // Should be defined as same size.
    EXPECT_EQ(source_buffer_.size(), flash_.buffer().size_bytes());

    // Can't allow it to march off the end of source_buffer_.
    ASSERT_LE(verify_bytes.size_bytes(), source_buffer_.size());

    for (size_t i = 0; i < verify_bytes.size_bytes(); i++) {
      EXPECT_EQ(source_buffer_[i], verify_bytes[i]);
    }
  }

  static constexpr size_t kFlashAlignment = 16;
  static constexpr size_t kSectorSize = 2048;
  static constexpr size_t kSectorCount = 2;

  kvs::FakeFlashMemoryBuffer<kSectorSize, kSectorCount> flash_;
  kvs::FlashPartition partition_;
  std::array<std::byte, kSectorCount * kSectorSize> source_buffer_;
};

TEST_F(BlobStoreTest, Init_Ok) {
  BlobStoreBuffer<256> blob("Blob_OK", &partition_, nullptr, kvs::TestKvs());
  EXPECT_EQ(Status::OK, blob.Init());
}

TEST_F(BlobStoreTest, MultipleErase) {
  BlobStoreBuffer<256> blob("Blob_OK", &partition_, nullptr, kvs::TestKvs());
  EXPECT_EQ(Status::OK, blob.Init());

  BlobStore::BlobWriter writer(blob);
  EXPECT_EQ(Status::OK, writer.Open());

  EXPECT_EQ(Status::OK, writer.Erase());
  EXPECT_EQ(Status::OK, writer.Erase());
  EXPECT_EQ(Status::OK, writer.Erase());
}

TEST_F(BlobStoreTest, ChunkWrite1) {
  InitSourceBufferToRandom(0x8675309);
  ChunkWriteTest(1);
}

TEST_F(BlobStoreTest, ChunkWrite2) {
  InitSourceBufferToRandom(0x8675);
  ChunkWriteTest(2);
}

TEST_F(BlobStoreTest, ChunkWrite3) {
  InitSourceBufferToFill(0);
  ChunkWriteTest(3);
}

TEST_F(BlobStoreTest, ChunkWrite4) {
  InitSourceBufferToFill(1);
  ChunkWriteTest(4);
}

TEST_F(BlobStoreTest, ChunkWrite5) {
  InitSourceBufferToFill(0xff);
  ChunkWriteTest(5);
}

TEST_F(BlobStoreTest, ChunkWrite16) {
  InitSourceBufferToRandom(0x86);
  ChunkWriteTest(16);
}

TEST_F(BlobStoreTest, ChunkWrite64) {
  InitSourceBufferToRandom(0x9);
  ChunkWriteTest(64);
}

TEST_F(BlobStoreTest, ChunkWrite256) {
  InitSourceBufferToRandom(0x12345678);
  ChunkWriteTest(256);
}

TEST_F(BlobStoreTest, ChunkWrite512) {
  InitSourceBufferToRandom(0x42);
  ChunkWriteTest(512);
}

TEST_F(BlobStoreTest, ChunkWrite4096) {
  InitSourceBufferToRandom(0x89);
  ChunkWriteTest(4096);
}

TEST_F(BlobStoreTest, ChunkWriteSingleFull) {
  InitSourceBufferToRandom(0x98765);
  ChunkWriteTest((kSectorCount * kSectorSize));
}

// TODO: test that does close with bytes still in the buffer to test zero fill
// to alignment and write on close.

// TODO: test to do write/close, write/close multiple times.

// TODO: test start with old blob (with KVS entry), open, invalidate, no writes,
// close. Verify the KVS entry is gone and blob is fully invalid.

// TODO: test that checks doing read after bytes are in write buffer but before
// any bytes are flushed to flash.

// TODO: test mem mapped read when bytes in write buffer but nothing written to
// flash.

}  // namespace
}  // namespace pw::blob_store
