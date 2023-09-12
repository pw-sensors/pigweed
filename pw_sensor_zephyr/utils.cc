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

#include "pw_sensor_zephyr/utils.h"

enum sensor_channel pw::sensor::zephyr::SensorTypeToChannel(
    pw::sensor::SensorType type) {
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
