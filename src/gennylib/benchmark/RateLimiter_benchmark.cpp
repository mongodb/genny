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

#include <boost/log/trivial.hpp>

#include <loki/ScopeGuard.h>

#include <gennylib/config/RateLimiterOptions.hpp>
#include <gennylib/conventions.hpp>
#include <gennylib/v1/RateLimiter.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

namespace Catchers = Catch::Matchers;

/**
 * So this benchmark tests the majority of the RateLimiter correctness and the performance of the
 * RateLimiterSimple. That said, it would be time-consuming but not difficult to produce
 * a RateLimiterMock that only advances a fake clock and verifies that.
 */
TEST_CASE("RateLimiter", "[benchmark]") {
    using ClockT = typename genny::v1::RateLimiter::ClockT;
    using Options = genny::v1::RateLimiter::Options;

    auto toMetricDuration = [](auto duration) -> auto {
        return std::chrono::duration_cast<genny::Duration>(duration);
    };

    // Simple counter with volatile int
    struct Counter {
        void incr() {
            ++count;
        }

        volatile int32_t count = 0;
    };

    auto testTotalDuration = [&](const Options& options, int64_t numLoops) {
        std::unique_ptr<genny::v1::RateLimiter> limiter =
            std::make_unique<genny::v1::RateLimiterSimple>(options);

        // P95
        auto expectedPeriod = std::max<genny::Duration>({options.preSleep + options.postSleep,
                                                         options.minPeriod,
                                                         std::chrono::nanoseconds{50}});
        auto maxDuration = numLoops * toMetricDuration(expectedPeriod) * 1050 / 1000;

        auto counter = Counter{};

        auto start = ClockT::now();

        for (auto i = 0ll; i < numLoops; ++i) {
            limiter->run([&] { counter.incr(); });
        }
        auto end = ClockT::now();

        auto duration = toMetricDuration(end - start);
        REQUIRE(duration.count() < maxDuration.count());
        BOOST_LOG_TRIVIAL(info) << numLoops << "ops/" << duration.count() << "ns";

        REQUIRE(counter.count == numLoops);
    };

    auto testPeriodDuration = [&](const Options& options, int64_t numLoops) {
        std::unique_ptr<genny::v1::RateLimiter> limiter =
            std::make_unique<genny::v1::RateLimiterSimple>(options);

        // P95 because we can start late on the new run
        auto expectedPeriod = std::max<genny::Duration>({options.preSleep + options.postSleep,
                                                         options.minPeriod,
                                                         std::chrono::nanoseconds{50}});
        auto periodThreshold = toMetricDuration(expectedPeriod) * 950 / 1000;

        auto counter = Counter{};

        // Run once to get our initial out of the way
        auto last = ClockT::now();
        auto gap = genny::Duration::zero();
        limiter->run([&] {
            counter.incr();

            last = ClockT::now();
        });

        for (auto i = 1ll; i < numLoops; ++i) {
            limiter->run([&] {
                counter.incr();

                auto now = ClockT::now();
                gap = toMetricDuration(now - last);
                last = now;
            });

            REQUIRE(gap.count() > periodThreshold.count());
        }

        REQUIRE(counter.count == numLoops);
    };

    // Note that all of these tests are tuned towards running in 1sec
    SECTION("Does not limit with default options") {
        auto constexpr kNumLoops = 20ll * 1000ll * 1000ll;
        testTotalDuration(Options{}, kNumLoops);
    }

    SECTION("Limit with a few periods") {
        SECTION("1ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{1ll};

            auto constexpr kNumLoops = 1000ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("10ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("100ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{100ll};

            auto constexpr kNumLoops = 10ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("999ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{999ll};

            auto constexpr kLogStr = "999ms period";
            auto constexpr kNumLoops = 2ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }
    }

    SECTION("Limit with 10ms period and various sleeps") {
        SECTION("5ms preSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.preSleep = std::chrono::milliseconds{5ll};

            auto constexpr kLogStr = "5ms preSleep";
            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("15ms preSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.preSleep = std::chrono::milliseconds{15ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("5ms postSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.postSleep = std::chrono::milliseconds{5ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("15ms postSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.postSleep = std::chrono::milliseconds{15ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("5ms preSleep and 5ms postSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.preSleep = std::chrono::milliseconds{5ll};
            options.postSleep = std::chrono::milliseconds{5ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }

        SECTION("6ms preSleep and 6ms postSleep") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};
            options.preSleep = std::chrono::milliseconds{5ll};
            options.postSleep = std::chrono::milliseconds{5ll};

            auto constexpr kNumLoops = 100ll;
            testTotalDuration(options, kNumLoops);
            testPeriodDuration(options, kNumLoops);
        }
    }
}
