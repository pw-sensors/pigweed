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

pw_add_library(pw_splitbuf STATIC
  HEADERS
    public/pw_splitbuf/splitbuf.h
  PUBLIC_INCLUDES
    public
  SOURCES
    splitbuf.cc
  PUBLIC_DEPS
    pw_sync.mutex
    pw_allocator.freelist_heap
    pw_bytes
    pw_preprocessor
    pw_result
    pw_span
    pw_status
)

pw_add_test(pw_splitbuf.test
  SOURCES
    splitbuf_test.cc
  PRIVATE_DEPS
    pw_splitbuf
  GROUPS
    modules
    pw_splitbuf
)
