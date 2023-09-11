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

#include "pw_containers/intrusive_list.h"
#include "pw_result/result.h"

namespace pw::sensor {

enum AttributeType {
  SAMPLE_RATE,
  COUNT,
};

struct Attribute :
    public pw::IntrusiveList<Attribute>::Item {
  AttributeType type;
  bool is_set;
  union {
    int64_t int_val;
    float float_val;
  } value;
  Attribute(AttributeType type) : type(type), is_set(false), value({0}) {}
  Attribute(const Attribute &other) : type(other.type), is_set(other.is_set), value(other.value) {
  }

  Attribute &operator=(const Attribute &rhs) {
    type = rhs.type;
    is_set = rhs.is_set;
    value = rhs.value;
    return *this;
  }

  template<typename T>
  operator Result<T>() const {
    if (!is_set) {
      return pw::Result<T>(pw::Status::Unavailable());
    } else if (std::is_integral<T>::value) {
      return pw::Result<T>(value.int_val);
    } else if (std::is_floating_point<T>::value) {
      return pw::Result<T>(value.float_val);
    }
  }
};

}  // namespace pw::sensor
