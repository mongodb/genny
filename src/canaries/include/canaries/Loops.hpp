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

#ifndef HEADER_C8D12C92_7C08_4B7F_9857_8B25F7258BB9_INCLUDED
#define HEADER_C8D12C92_7C08_4B7F_9857_8B25F7258BB9_INCLUDED


#include <cstdint>

#include <canaries/Workloads.hpp>

namespace genny::canaries {

using Nanosecond = int64_t;

/**
 * Class for benchmarking Genny internals. It's recommended to run
 * these benchmarks as canaries before running Genny workloads.
 *
 * @tparam WithPing whether to issue db.ping() to a mongo cluster or just
 *                  do a simple increment. The former is useful for gauging
 *                  the overhead of Genny and the latter is useful for seeing
 *                  how this overhead changes over time.
 */
template <class Task, class... Args>
class Loops {
public:
    explicit Loops(int64_t iterations) : _iterations(iterations){};

    /**
     * Run native for-loop; used as the control group with no Genny code.
     */
    Nanosecond simpleLoop(Args&&... args);

    /**
     * Run PhaseLoop.
     */
    Nanosecond phaseLoop(Args&&... args);

    /**
     *  Run native for-loop and record one timer metric per iteration.
     */
    Nanosecond metricsLoop(Args&&... args);

    /**
     * Run PhaseLoop and record one timer metric per iteration.
     */
    Nanosecond metricsPhaseLoop(Args&&... args);

private:
    int64_t _iterations;
};
}  // namespace genny::canaries

#endif  // HEADER_C8D12C92_7C08_4B7F_9857_8B25F7258BB9_INCLUDED
