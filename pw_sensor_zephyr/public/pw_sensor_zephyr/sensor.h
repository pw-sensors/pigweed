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

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/rtio/rtio.h>

#include "pw_containers/intrusive_list.h"
#include "pw_sensor_zephyr/decoder.h"
#include "pw_sensor/sensor.h"

namespace pw::sensor::zephyr {

class ZephyrSensor : public pw::sensor::Sensor {
 public:
  friend class Request;

  ZephyrSensor(const struct device* dev, const struct sensor_decoder_api *decoder_api = nullptr);
  ~ZephyrSensor() = default;

  pw::Status SetConfiguration(const Configuration& config);
  pw::Result<Configuration> GetConfiguration();

  pw::sensor::SensorFuture Read(SensorContext& context,
                                pw::span<SensorType> types) override;

  pw::sensor::Decoder& GetDecoder() { return decoder_; }

 protected:
  const struct device* device_;
  enum sensor_channel read_channels_[1];
  struct sensor_read_config read_config_;
  struct rtio_iodev iodev_;
  Configuration configuration_;
  Decoder decoder_;
};

}  // namespace pw::sensor::zephyr
