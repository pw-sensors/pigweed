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

#include "pw_chrono/system_clock.h"

namespace pw::chrono::zephyr {

// Max timeout to be used by users of the Zephyr pw::chrono::SystemClock
// backend provided by this module.
inline constexpr SystemClock::duration kMaxTimeout =
    SystemClock::duration(K_FOREVER.ticks - 1);

}  // namespace pw::chrono::zephyr
