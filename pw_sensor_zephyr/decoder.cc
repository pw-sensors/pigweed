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

#include "pw_sensor_zephyr/decoder.h"

#include "pw_sensor_zephyr/utils.h"

pw::Result<size_t> pw::sensor::zephyr::Decoder::GetFrameCount(pw::sensor::DecoderContext& ctx,
    SensorType type, size_t type_index) {
  if (api_->get_frame_count == nullptr) {
    return pw::Status::Unimplemented();
  }

  uint16_t frame_count;
  int rc = api_->get_frame_count((const uint8_t*)ctx.buffer.data(),
                                 SensorTypeToChannel(type),
                                 type_index,
                                 &frame_count);
  if (rc != 0) {
    return pw::Status::Internal();
  }
  return {static_cast<size_t>(frame_count)};
}

pw::Result<pw::sensor::SizeInfo> pw::sensor::zephyr::Decoder::GetSizeInfo(DecoderContext& ,
    pw::sensor::SensorType type) {
  if (api_->get_size_info == nullptr) {
    return pw::Status::Unimplemented();
  }

  pw::sensor::SizeInfo size_info;
  int rc = api_->get_size_info(
      SensorTypeToChannel(type), &size_info.base_size, &size_info.frame_size);
  if (rc != 0) {
    return pw::Status::Internal();
  }
  return size_info;
}

pw::Result<size_t> pw::sensor::zephyr::Decoder::Decode(DecoderContext& ctx,
    pw::sensor::SensorType type,
    size_t type_index,
    size_t max_count,
    pw::ByteSpan out) {
  if (api_->decode == nullptr) {
    return pw::Status::Unimplemented();
  }

  int rc = api_->decode((const uint8_t*)ctx.buffer.data(),
                        SensorTypeToChannel(type),
                        type_index,
                        &ctx.frame_iterator,
                        max_count,
                        out.data());

  if (rc < 0) {
    return pw::Status::Internal();
  }
  return rc;
}
