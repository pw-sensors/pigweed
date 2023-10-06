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

#define PW_LOG_MODULE_NAME "Sensor"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_sensor_zephyr/sensor.h"

#include <zephyr/drivers/sensor.h>

#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_sensor_zephyr/decoder.h"
#include "pw_sensor_zephyr/utils.h"

namespace {
inline float SensorValueToFloat(const struct sensor_value& value) {
  return static_cast<float>(value.val1) +
         static_cast<float>(value.val2) / 1000000.0f;
}

inline struct sensor_value FloatToSensorValue(float value) {
  return {
      .val1 = static_cast<int32_t>(value),
      .val2 = static_cast<int32_t>((value - static_cast<int32_t>(value)) *
                                   1000000.0f),
  };
}

}  // namespace

namespace pw::sensor::zephyr {

ZephyrSensor::ZephyrSensor(const struct device* dev,
                           const struct sensor_decoder_api* decoder_api)
    : device_(dev),
      read_config_({.sensor = dev,
                    .channels = read_channels_,
                    .count = 0,
                    .max = ARRAY_SIZE(read_channels_)}),
      iodev_({.api = &__sensor_iodev_api,
              .iodev_sq = RTIO_MPSC_INIT(iodev_.iodev_sq),
              .data = &read_config_}) {
  if (decoder_api == nullptr) {
    // TODO maybe check the return value here?
    sensor_get_decoder(dev, &decoder_api);
  }
  decoder_ = pw::sensor::zephyr::Decoder(decoder_api);
  struct sensor_value value = {0, 0};
  for (auto sensor_type : SensorTypeList) {
    enum sensor_channel channel = SensorTypeToChannel(sensor_type);
    if (sensor_attr_get(
            device_, channel, SENSOR_ATTR_SAMPLING_FREQUENCY, &value) != 0) {
      continue;
    }
    configuration_.SetSampleRate(sensor_type, SensorValueToFloat(value));
  }
}

pw::Status ZephyrSensor::SetConfiguration(const Configuration& config) {
  ARG_UNUSED(config);
  for (auto sensor_type : SensorTypeList) {
    if (!configuration_.HasSampleRate(sensor_type) ||
        !config.IsSampleRateDirty(sensor_type)) {
      continue;
    }

    enum sensor_channel channel = SensorTypeToChannel(sensor_type);
    struct sensor_value value =
        FloatToSensorValue(config.GetSampleRate(sensor_type));
    if (sensor_attr_set(
            device_, channel, SENSOR_ATTR_SAMPLING_FREQUENCY, &value) != 0) {
      return pw::Status::Unknown();
    }
  }
  return pw::OkStatus();
}

pw::Result<Configuration> ZephyrSensor::GetConfiguration() {
  return {configuration_};
}

pw::sensor::SensorFuture ZephyrSensor::Read(pw::sensor::SensorContext& ctx,
                                            pw::span<SensorType> types) {
  auto handle = ctx.NextRequestId();

  read_config_.channels[0] = SensorTypeToChannel(types[0]);
  read_config_.count = 1;

  if (sensor_read(&iodev_, ctx.native_type().r_, (void*)handle) != 0) {
    // Failed to start the read
    return SensorFuture(pw::Status::Internal());
  }

  SensorFuture future(&ctx, this, handle);
  ctx.AddFuture(future);
  return future;
}

}  // namespace pw::sensor::zephyr

static pw::Result<pw::allocator::experimental::Block> ProcessCQE(
    pw::sensor::backend::NativeSensorContext& ctx, struct rtio_cqe* cqe) {
  if (cqe->result < 0) {
    rtio_cqe_release(ctx.r_, cqe);
    return pw::Status::Internal();
  }

  uint8_t* buffer;
  uint32_t buffer_len;
  int rc = rtio_cqe_get_mempool_buffer(ctx.r_, cqe, &buffer, &buffer_len);

  rtio_cqe_release(ctx.r_, cqe);
  if (rc != 0) {
    return pw::Status::Internal();
  }

  return pw::allocator::experimental::Block(
      ctx.allocator_, pw::ByteSpan({(std::byte*)buffer, buffer_len}));
}

pw::Status pw::sensor::SensorContext::NotifyComplete(
    pw::sensor::backend::NativeSensorFutureResultType result) {
  for (auto& request : pending_requests_) {
    if (!request.IsWaitingOn(this, (std::uintptr_t)result->userdata)) {
      continue;
    }

    request.SetResult(ProcessCQE(native_type_, result));
    return pw::OkStatus();
  }
  return pw::Status::Unavailable();
}

pw::async::Poll<pw::Result<pw::allocator::experimental::Block>>
pw::sensor::SensorFuture::Poll(pw::async::Waker& waker) {
  if (value_.is_b()) {
    ctx_ = nullptr;
    return {std::move(value_.value_b())};
  }

  // TODO this should loop until returning nullptr or matching handle_
  struct rtio_cqe* cqe;
  bool found = false;
  pw::Result<pw::allocator::experimental::Block> result(pw::Status::Unknown());

  while ((cqe = rtio_cqe_consume(ctx_->native_type().r_)) != nullptr) {
    if ((std::uintptr_t)cqe->userdata != handle_) {
      // CQE doesn't belong to us, send it to the context
      PW_ASSERT_OK(ctx_->NotifyComplete(cqe));
      continue;
    }

    // CQE belongs to us
    PW_ASSERT(!found);
    found = true;
    result = ProcessCQE(ctx_->native_type(), cqe);
  }
  if (!found) {
    waker_ = waker;
    return pw::async::Pending();
  }
  ctx_ = nullptr;
  return result;
}
