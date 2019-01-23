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
#include "test.h"

#include <chrono>
#include <ratio>

#include "log.hh"

namespace genny::testing {
struct DummyClock {

    // <clock-concept>
    using rep = int64_t;
    using period = std::nano;
    using duration = std::chrono::duration<DummyClock::rep, DummyClock::period>;
    using time_point = std::chrono::time_point<DummyClock>;
    const static bool is_steady = true;
    // </clock-concept>

    int64_t nowRaw = 0;

    auto now() {
        return DummyClock::time_point(DummyClock::duration(nowRaw));
    }
};

namespace {
TEST_CASE("Dummy Clock self-test") {
    auto getTicks = [](const DummyClock::duration& d) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
    };
    DummyClock clock;
    SECTION("Can be converted to nano seconds") {
        auto now = clock.now().time_since_epoch();
        REQUIRE(getTicks(now) == 0);

        clock.nowRaw++;
        now = clock.now().time_since_epoch();
        REQUIRE(getTicks(now) == 1);
    }
}
}  // namespace
}  // namespace genny::testing
