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

#include "pw_sensor/configuration.h"
#include "pw_sensor/types.h"
#include "pw_span/span.h"

namespace pw::sensor {

class Sensor {
 public:
  Sensor() = default;
  virtual ~Sensor() {}
  Sensor(const Sensor&) = delete;
  Sensor& operator=(const Sensor&) = delete;

  virtual pw::Status SetConfiguration(const Configuration &cofig) = 0;
  virtual pw::Result<Configuration> GetConfiguration() = 0;

  virtual pw::Status Read(pw::span<SensorType> types) = 0;
};

}  // namespace pw::sensor
