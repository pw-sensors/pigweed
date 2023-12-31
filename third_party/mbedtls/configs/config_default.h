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

#include <mbedtls/version.h>

#if MBEDTLS_VERSION_MAJOR >= 3
#include <mbedtls/build_info.h>
#include <mbedtls/mbedtls_config.h>
#else
#include <mbedtls/config.h>
#endif

// override some flags needed by pigweed
#include "configs/config_pigweed_common.h"
