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

/**
 * Class for benchmarking Genny internals. It's recommended to run
 * these benchmarks as canaries before running Genny workloads.
 *
 * @tparam WithPing whether to issue db.ping() to a mongo cluster or just
 *                  do a simple increment. The former is useful for gauging
 *                  the overhead of Genny and the latter is useful for seeing
 *                  how this overhead changes over time.
 */
template <bool WithPing>
class Loops {
public:
    explicit Loops(int64_t iterations) : _iterations(iterations){};

    /**
     * Run a basic for-loop, used as the baseline.
     */
    int64_t simpleLoop();

    /**
     * Run just the phase loop.
     */
    int64_t phaseLoop();

    /**
     * Run a basic for-loop and record one timer metric per loop.
     */
    int64_t metricsLoop();

    /**
     * Run the phase loop and record one timer metric per loop.
     */
    int64_t metricsPhaseLoop();

private:
    int64_t _iterations;
};
}  // namespace genny::canaries
