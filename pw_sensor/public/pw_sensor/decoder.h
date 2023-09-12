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

#include <inttypes.h>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_sensor/types.h"
#include "pw_sensor_zephyr/sensor_native.h"
#include "pw_status/status.h"

namespace pw::sensor {

struct SizeInfo {
  size_t base_size;
  size_t frame_size;
};

class Decoder {
 public:
  Decoder() = default;
  virtual ~Decoder() = default;

  virtual pw::Status Reset(pw::ConstByteSpan buffer) {
    buffer_ = buffer;
    return pw::OkStatus();
  }
  virtual pw::Result<size_t> GetFrameCount(SensorType type, size_t type_index) = 0;

  virtual pw::Result<SizeInfo> GetSizeInfo(SensorType type) = 0;

  virtual pw::Result<size_t> Decode(SensorType type,
                    size_t type_index,
                    size_t max_count,
                    pw::ByteSpan out) = 0;

 protected:
  pw::ConstByteSpan buffer_;
};

}  // namespace pw::sensor
