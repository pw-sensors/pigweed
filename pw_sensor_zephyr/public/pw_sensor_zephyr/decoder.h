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

#include <zephyr/drivers/sensor.h>

#include "pw_sensor/decoder.h"

namespace pw::sensor::zephyr {

class Decoder : public pw::sensor::Decoder {
 public:
  Decoder(const struct sensor_decoder_api* api = nullptr)
      : api_(api) {}
  ~Decoder() = default;

  pw::Result<size_t> GetFrameCount(DecoderContext& ctx,
                                   SensorType type,
                                   size_t type_index) override;

  pw::Result<SizeInfo> GetSizeInfo(DecoderContext& ctx,
                                   SensorType type) override;

  pw::Result<size_t> Decode(DecoderContext& ctx,
                            SensorType type,
                            size_t type_index,
                            size_t max_count,
                            pw::ByteSpan out) override;

 private:
  const struct sensor_decoder_api* api_;
};

}  // namespace pw::sensor::zephyr
