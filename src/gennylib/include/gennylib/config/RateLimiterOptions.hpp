// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
#define HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4

#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::config {
struct RateLimiterOptions {
    Duration minPeriod = Duration::zero();
    Duration preSleep = Duration::zero();
    Duration postSleep = Duration::zero();
};
}  // namespace genny::config

#endif  // HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
