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

#include <string.h>

#include "pw_containers/intrusive_list.h"
#include "pw_sensor/attribute.h"
#include "pw_sensor/types.h"
#include "pw_status/status.h"
#include "pw_result/result.h"
#include "pw_span/span.h"

namespace pw::sensor {

class Configuration {
 public:

  Configuration() {
    memset(sample_rate_, 0, sizeof(sample_rate_));
  }
  Configuration(const Configuration &rhs) {
    memcpy(sample_rate_, rhs.sample_rate_, sizeof(sample_rate_));
    for (auto sample_rate : sample_rate_) {
      sample_rate.is_dirty = false;
    }
  }

  inline bool HasSampleRate(SensorType type) const {
    return sample_rate_[type].is_enabled;
  }
  inline bool IsSampleRateDirty(SensorType type) const {
    return sample_rate_[type].is_dirty;
  }
  inline float GetSampleRate(SensorType type) const {
    return sample_rate_[type].value;
  }
  inline void SetSampleRate(SensorType type, float rate) {
    sample_rate_[type].is_enabled = true;
    sample_rate_[type].is_dirty = true;
    sample_rate_[type].value = rate;
  }
 protected:
  struct {
    bool is_enabled;
    bool is_dirty;
    float value;
  } sample_rate_[SensorType::NUM_ENTRIES];
};

}  // namespace pw::sensor
