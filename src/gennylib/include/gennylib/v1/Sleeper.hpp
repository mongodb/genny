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

#ifndef HEADER_A2EF8CA3_1185_4B6C_979B_BD40D6893EF8_INCLUDED
#define HEADER_A2EF8CA3_1185_4B6C_979B_BD40D6893EF8_INCLUDED

#include <chrono>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/conventions.hpp>


namespace genny::v1 {

/**
 * Class for sleeping before and after an operation. Checks that the current phase has
 * not ended before sleeping.
 */
class Sleeper {
    using Duration = genny::Duration;

public:
    /**
     * Default constructs a no-op sleeper.
     */
    Sleeper() : _before(Duration::zero()), _after(Duration::zero()){};

    /**
     * Construct a sleeper object.
     * @param before time to sleep before an operation.
     * @param after time to sleep after an operation.
     */
    Sleeper(Duration before, Duration after) : _before(before), _after(after){};

    // No copies or moves.
    Sleeper(const Sleeper& other) = delete;
    Sleeper& operator=(const Sleeper& other) = delete;

    Sleeper(Sleeper&& other) = delete;
    Sleeper& operator=(Sleeper&& other) = delete;

    constexpr void sleepFor(Orchestrator& orchestrator,
                            const PhaseNumber phase,
                            const Duration period,
                            bool phaseChangeWakeup = false) const {

        if (phaseChangeWakeup) {
            // Using locks / condition variables is less efficient/safe, so we
            // only use this mechanism if the caller explicitly asked for it.
            orchestrator.sleepToPhaseEnd(period, phase);
        } else if (period.count() > 0 && orchestrator.currentPhase() == phase) {
            std::this_thread::sleep_for(period);
        }
    }

    /**
     * Sleep for duration before an operation. Checks that the current phase has
     * not ended before sleeping.
     */
    constexpr void before(const Orchestrator& orchestrator, const PhaseNumber phase) const {
        if (_before.count() > 0 && orchestrator.currentPhase() == phase) {
            std::this_thread::sleep_for(_before);
        }
    }

    /**
     * @see Sleeper::before
     */
    constexpr void after(const Orchestrator& orchestrator, const PhaseNumber phase) const {
        if (_after.count() > 0 && orchestrator.currentPhase() == phase) {
            std::this_thread::sleep_for(_after);
        }
    }

private:
    Duration _before;
    Duration _after;
};

}  // namespace genny::v1

#endif  // HEADER_A2EF8CA3_1185_4B6C_979B_BD40D6893EF8_INCLUDED
