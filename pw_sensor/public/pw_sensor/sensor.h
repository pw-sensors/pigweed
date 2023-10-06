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

#include "pw_async/dispatcher.h"
#include "pw_bytes/span.h"
#include "pw_containers/intrusive_list.h"
#include "pw_sensor/configuration.h"
#include "pw_sensor/poll.h"
#include "pw_sensor/types.h"
#include "pw_sensor/decoder.h"
#include "pw_sensor_backend/sensor_native.h"
#include "pw_span/span.h"

namespace pw::sensor {

class Sensor;
class SensorContext;

class SensorFuture : public pw::IntrusiveList<SensorFuture>::Item {
 public:
  SensorFuture(SensorContext* ctx, Sensor *sensor, std::uintptr_t handle)
      : ctx_(ctx),
        sensor_(sensor),
        value_(std::in_place_type<async::Pending>),
        handle_(handle) {}
  SensorFuture(pw::Status status) : ctx_(nullptr), value_(status) {}
  SensorFuture(SensorFuture&) = delete;
  SensorFuture(SensorFuture&& other)
      : ctx_(other.ctx_),
        sensor_(other.sensor_),
        value_(std::move(other.value_)),
        handle_(other.handle_),
        waker_(other.waker_) {
    other.ctx_ = nullptr;
    other.sensor_ = nullptr;
    other.value_ = pw::Status::Cancelled();
  }

  bool IsWaitingOn(SensorContext* ctx, std::uintptr_t handle) {
    return value_.is_a() && ctx_ == ctx && handle_ == handle;
  }

  void SetResult(pw::Result<pw::allocator::experimental::Block>&& result) {
      PW_ASSERT(value_.is_a());
      value_ = {std::move(result)};
      waker_.Wake();
  }

  pw::async::Poll<pw::Result<pw::allocator::experimental::Block>> Poll(pw::async::Waker& waker);

  Sensor* sensor() { return sensor_; }

 private:
  SensorContext* ctx_;
  Sensor *sensor_;
  bivariant<async::Pending, pw::Result<pw::allocator::experimental::Block>> value_;
  std::uintptr_t handle_;

  pw::async::Waker waker_;
};

class SensorContext {
 public:
  SensorContext(backend::NativeSensorContext native_type)
      : native_type_(native_type) {}
  void AddFuture(SensorFuture& future) { pending_requests_.push_back(future); }
  pw::Status NotifyComplete(backend::NativeSensorFutureResultType result);
  std::uintptr_t NextRequestId() { return next_request_id_++; }
  inline backend::NativeSensorContext &native_type() {
    return native_type_;
  }

 protected:
  backend::NativeSensorContext native_type_;
  std::uintptr_t next_request_id_ = 0;

  pw::IntrusiveList<SensorFuture> pending_requests_;
};

class Sensor {
 public:
  Sensor() = default;
  virtual ~Sensor() {}
  Sensor(const Sensor&) = delete;
  Sensor& operator=(const Sensor&) = delete;

  virtual pw::Status SetConfiguration(const Configuration& cofig) = 0;
  virtual pw::Result<Configuration> GetConfiguration() = 0;

  virtual SensorFuture Read(SensorContext& context,
                            pw::span<SensorType> types) = 0;

  virtual Decoder& GetDecoder() = 0;
};

}  // namespace pw::sensor
