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

#include "pw_sensor_zephyr/sensor.h"
#include <zephyr/drivers/sensor.h>

namespace {
inline float SensorValueToFloat(const struct sensor_value &value) {
  return static_cast<float>(value.val1) + static_cast<float>(value.val2) / 1000000.0f;
}

inline struct sensor_value FloatToSensorValue(float value) {
  return {
      .val1 = static_cast<int32_t>(value),
      .val2 = static_cast<int32_t>((value - static_cast<int32_t>(value)) * 1000000.0f),
  };
}

inline enum sensor_channel SensorTypeToChannel(pw::sensor::SensorType type) {
  switch (type) {
    case pw::sensor::SensorType::ACCELEROMETER:
      return SENSOR_CHAN_ACCEL_XYZ;
    case pw::sensor::SensorType::GYROSCOPE:
      return SENSOR_CHAN_GYRO_XYZ;
    case pw::sensor::SensorType::MAGNETOMETER:
      return SENSOR_CHAN_MAGN_XYZ;
    default:
      return SENSOR_CHAN_ALL;
  }
}
}

namespace pw::sensor::zephyr {

ZephyrSensor::ZephyrSensor(const struct device *dev)
    : device_(dev),
      read_config_({.sensor = dev, .channels = read_channels_, .count = 0, .max = ARRAY_SIZE(read_channels_)}),
      iodev_({.api = &__sensor_iodev_api, .iodev_sq = RTIO_MPSC_INIT(iodev_.iodev_sq), .data = &read_config_}) {
  struct sensor_value value = {0, 0};
  for (auto sensor_type : SensorTypeList) {
    enum sensor_channel channel = SensorTypeToChannel(sensor_type);
    if (sensor_attr_get(device_, channel, SENSOR_ATTR_SAMPLING_FREQUENCY, & value) != 0) {
      continue;
    }
    configuration_.SetSampleRate(sensor_type, SensorValueToFloat(value));
  }
}

pw::Status ZephyrSensor::SetConfiguration(const Configuration &config) {
  ARG_UNUSED(config);
  for (auto sensor_type : SensorTypeList) {
    if (!configuration_.HasSampleRate(sensor_type) || !config.IsSampleRateDirty(sensor_type)) {
      continue;
    }

    enum sensor_channel channel = SensorTypeToChannel(sensor_type);
    struct sensor_value value = FloatToSensorValue(config.GetSampleRate(sensor_type));
    if (sensor_attr_set(device_, channel, SENSOR_ATTR_SAMPLING_FREQUENCY, &value) != 0) {
      return pw::Status::Unknown();
    }
  }
  return pw::OkStatus();
}

pw::Result<Configuration> ZephyrSensor::GetConfiguration() {
  return {configuration_};
}

pw::Status ZephyrSensor::Read(pw::span<SensorType> types) {
  read_config_.channels[0] = SensorTypeToChannel(types[0]);
  sensor_read(&iodev_, nullptr, this);
  return pw::Status::Unimplemented();
}

}  // namespace pw::sensor::zephyr
