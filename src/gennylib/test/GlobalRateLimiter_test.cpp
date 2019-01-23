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

#include <gennylib/v1/GlobalRateLimiter.hpp>

namespace genny::testing {

// Hack to allow different DummyClocks classes to be created so calling static methods won't
// conflict.
template <typename DummyT>
struct DummyClock {
    static int64_t nowRaw;

    // <clock-concept>
    using rep = int64_t;
    using period = std::nano;
    using duration = std::chrono::duration<DummyClock::rep, DummyClock::period>;
    using time_point = std::chrono::time_point<DummyClock>;
    const static bool is_steady = true;
    // </clock-concept>

    static auto now() {
        return DummyClock::time_point(DummyClock::duration(nowRaw));
    }
};

template <typename DummyT>
int64_t DummyClock<DummyT>::nowRaw = 0;

namespace {
TEST_CASE("Dummy Clock") {
    struct DummyTemplateValue {};
    using MyDummyClock = DummyClock<DummyTemplateValue>;

    auto getTicks = [](const MyDummyClock::duration& d) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
    };

    SECTION("Can be converted to time points") {
        auto now = MyDummyClock::now().time_since_epoch();
        REQUIRE(getTicks(now) == 0);

        MyDummyClock::nowRaw++;
        now = MyDummyClock::now().time_since_epoch();
        REQUIRE(getTicks(now) == 1);
    }
}

TEST_CASE("Global rate limiter") {
    struct DummyTemplateValue {};
    using MyDummyClock = DummyClock<DummyTemplateValue>;

    const int64_t per = 3;
    const int64_t burst = 2;
    const RateSpec rs{per, burst};  // 2 operations per 3 ticks.
    v1::BaseGlobalRateLimiter<MyDummyClock> grl{rs};

    SECTION("Stores the burst size and rate") {
        REQUIRE(grl.getBurstSize() == burst);
        REQUIRE(grl.getRate() == per);
    }

    SECTION("Limits Rate") {
        // First consume() goes through because we intentionally have the rate limiter do so.
        REQUIRE(grl.consume());

        // Second consume() should fail because we have not incremented the clock.
        REQUIRE(!grl.consume());

        // Increment the clock should allow consume() to succeed exactly once.
        MyDummyClock::nowRaw = per;
        REQUIRE(grl.consume());
        REQUIRE(!grl.consume());
    }
}
}  // namespace
}  // namespace genny::testing
