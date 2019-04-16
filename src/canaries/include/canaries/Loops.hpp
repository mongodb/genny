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

#include <iostream>

#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>


namespace genny::canaries {

template <bool WithPing>
class Loops {
public:
    Loops() = default;

    int64_t simpleLoop(int64_t iterations);

    int64_t phaseLoop(int64_t iterations);

    int64_t metricsLoop(int64_t iterations);

    int64_t metricsPhaseLoop(int64_t iterations);
};
}  // namespace genny::canaries
