// Copyright 2021 The Pigweed Authors
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

#include <zephyr/kernel.h>
#include <zephyr/sys/mutex.h>

#include "pw_chrono/system_clock.h"
#include "pw_function/function.h"

namespace pw::chrono::backend {

struct NativeSystemTimer;

struct ZephyrWorkWrapper {
  k_work_delayable work;
  NativeSystemTimer* owner;
};

struct NativeSystemTimer {
  ZephyrWorkWrapper work_wrapper;
  sys_mutex mutex;
  SystemClock::time_point expiry_deadline;
  Function<void(SystemClock::time_point expired_deadline)> user_callback;
};
using NativeSystemTimerHandle = NativeSystemTimer&;

}  // namespace pw::chrono::backend
