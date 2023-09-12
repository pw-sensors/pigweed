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

#include <utility>

#include "pw_async/dispatcher.h"
#include "pw_async/task.h"
#include "pw_sensor/bivariant.h"

namespace pw::async {

template <typename T>
struct [[nodiscard(
    "`Poll`-returning functions may or may not have completed. Their return "
    "value should be examined.")]] Ready {
  T result;

  // Single-argument constructor to allow for in-place construction in
  // std::variant.
  Ready(T && result_value) : result(std::move(result_value)) {}
};

struct [[nodiscard(
    "`Poll`-returning functions may or may not have completed. Their return "
    "value should be examined.")]] Pending{};

template <typename T>
class [[nodiscard(
    "`Poll`-returning functions may or may not have completed. Their return "
    "value should be examined.")]] Poll {
 public:
  // Basic constructors.
  Poll() = delete;
  constexpr Poll(const Poll&) = default;
  constexpr Poll& operator=(const Poll&) = default;
  constexpr Poll(Poll &&) = default;
  constexpr Poll& operator=(Poll&&) = default;

  // In-place construction of `Ready` variant.
  template <typename... Args>
  constexpr Poll(std::in_place_type_t<T>, Args && ... args)
      : value_(std::in_place_type<Ready<T>>, std::forward<Args>(args)...) {}

  // Convert from `T`
  constexpr Poll(T && result)
      : value_(std::in_place_type<Ready<T>>, std::move(result)) {}
  constexpr Poll& operator=(T&& result) {
    value_ = bivariant<Ready<T>, Pending>(Ready{std::move(result)});
    return *this;
  }

  // Convert from `Ready<T>`
  constexpr Poll(Ready<T> && result) : value_(std::move(result)) {}
  constexpr Poll& operator=(Ready<T>&& result) {
    value_ = bivariant<Ready<T>, Pending>(std::move(result));
    return *this;
  }

  // Convert from `Pending`
  constexpr Poll(Pending) : value_(std::in_place_type<Pending>) {}
  constexpr Poll& operator=(Pending) {
    value_ = bivariant<Ready<T>, Pending>(Pending{});
    return *this;
  }

  constexpr bool IsReady() const { return value_.is_a(); }
  constexpr T& value()& { return value_.value_a().result; }
  constexpr const T& value() const& { return value_.value_a().result; }
  constexpr const T* operator->() const { return &value_.value_a().result; }
  constexpr T* operator->() { return &value_.value_a().result; }
  constexpr const T& operator*() const& { return value_.value_a().result; }
  constexpr T& operator*()& { return value_.value_a().result; }
  constexpr const T&& operator*() const&& {
    return std::move(value_.value_a().result);
  }
  constexpr T&& operator*()&& { return std::move(value_.value_a()); }

 private:
  bivariant<Ready<T>, Pending> value_;
};

class Waker {
 public:
  Waker() : dispatcher_(nullptr), task_(nullptr) {}
  Waker(Dispatcher& dispatcher, Task& task)
      : dispatcher_(&dispatcher), task_(&task) {}
  void Wake() {
    if (dispatcher_ != nullptr && task_ != nullptr) {
      dispatcher_->Post(*task_);
    }
  }

 private:
  Dispatcher* dispatcher_;
  Task* task_;
};

}  // namespace pw::async
