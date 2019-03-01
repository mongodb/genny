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
    Sleeper() : _before(0), _after(0){};

    /**
     * Construct a sleeper object.
     * @param before time to sleep before an operation.
     * @param after time to sleep after an operation.
     */
    Sleeper(Duration before, Duration after) : _before(before), _after(after) {
        const Duration zero(0);
        if (before < zero || after < zero) {
            throw InvalidConfigurationException("Duration cannot be negative");
        }
    }

    // No copies or moves.
    Sleeper(const Sleeper& other) = delete;
    Sleeper& operator=(const Sleeper& other) = delete;

    Sleeper(Sleeper&& other) = delete;
    Sleeper& operator=(Sleeper&& other) = delete;

    /**
     * Sleep for duration before an operation. Checks that the current phase has
     * not ended before sleeping.
     */
    constexpr void before(const Orchestrator& o, const PhaseNumber pn) const {
        if (_before.count() && o.currentPhase() == pn) {
            std::this_thread::sleep_for(_before);
        }
    }

    /**
     * @see Sleeper::before
     */
    constexpr void after(const Orchestrator& o, const PhaseNumber pn) const {
        if (_after.count() && o.currentPhase() == pn) {
            std::this_thread::sleep_for(_after);
        }
    }

private:
    Duration _before;
    Duration _after;
};

}  // namespace genny::v1

#endif  // HEADER_A2EF8CA3_1185_4B6C_979B_BD40D6893EF8_INCLUDED