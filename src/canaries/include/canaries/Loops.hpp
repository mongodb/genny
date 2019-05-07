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
#include <iomanip>

#include <canaries/tasks.hpp>

#include <gennylib/InvalidConfigurationException.hpp>

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
     *
     * @param args arguments forwarded to the workload being run.
     * @return the CPU time this function took, in nanoseconds.
     */
    Nanosecond simpleLoop(Args&&... args);

    /**
     * Run PhaseLoop.
     *
     * @param args arguments forwarded to the workload being run.
     * @return the CPU time this function took, in nanoseconds.
     */
    Nanosecond phaseLoop(Args&&... args);

    /**
     *  Run native for-loop and record one timer metric per iteration.
     *
     *  @param args arguments forwarded to the workload being run.
     *  @return the CPU time this function took, in nanoseconds.
     */
    Nanosecond metricsLoop(Args&&... args);

    /**
     * Run PhaseLoop and record one timer metric per iteration.
     *
     * @param args arguments forwarded to the workload being run.
     * @return the CPU time this function took, in nanoseconds.
     */
    Nanosecond metricsPhaseLoop(Args&&... args);

private:
    int64_t _iterations;
};

template <class Task, class... Args>
std::vector<Nanosecond> runTest(std::vector<std::string>& loopNames,
                                int64_t iterations,
                                Args&&... args) {
    Loops<Task, Args...> loops(iterations);

    std::vector<Nanosecond> results;

    int numIterations = 4;

    for (auto& loopName : loopNames) {
        Nanosecond time;

        // Run each test 4 times, all but the last iteration are warm up and the
        // results are discarded.
        for (int i = 0; i < numIterations; i++) {
            if (loopName == "simple") {
                time = loops.simpleLoop(std::forward<Args>(args)...);
            } else if (loopName == "phase") {
                time = loops.phaseLoop(std::forward<Args>(args)...);
            } else if (loopName == "metrics") {
                time = loops.metricsLoop(std::forward<Args>(args)...);
            } else if (loopName == "real") {
                time = loops.metricsPhaseLoop(std::forward<Args>(args)...);
            } else {
                std::ostringstream stm;
                stm << "Unknown loop type: " << loopName;
                throw InvalidConfigurationException(stm.str());
            }

            if (i == numIterations - 1) {
                results.push_back(time);
            }
        }
    }

    return results;
}

}  // namespace genny::canaries

#endif  // HEADER_C8D12C92_7C08_4B7F_9857_8B25F7258BB9_INCLUDED
