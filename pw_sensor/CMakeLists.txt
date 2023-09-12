# Copyright 2023 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_sensor/backend.cmake)

pw_add_library(pw_sensor.async INTERFACE
  HEADERS
    public/pw_sensor/bivariant.h
    public/pw_sensor/poll.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async.dispatcher
    pw_async.task
)

pw_add_facade(pw_sensor INTERFACE
  BACKEND
    pw_sensor_BACKEND
  HEADERS
    public/pw_sensor/attribute.h
    public/pw_sensor/configuration.h
    public/pw_sensor/decoder.h
    public/pw_sensor/sensor.h
    public/pw_sensor/types.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_sensor.async
    pw_containers
    pw_result
    pw_span
    pw_bytes
    pw_status
)
