#include "test.h"

#include "ActorHelper.hpp"

#include <iostream>

#include <boost/log/trivial.hpp>

#include <loki/ScopeGuard.h>

#include <gennylib/v1/RateLimiter.hpp>
#include <gennylib/config/RateLimiterOptions.hpp>

namespace Catchers = Catch::Matchers;

namespace genny::test {
}  // namespace genny::test

TEST_CASE("RateLimiter") {
    using ClockT = typename genny::v1::RateLimiter::ClockT;
    using Options = genny::v1::RateLimiter::Options;

    auto toMetricDuration = [](auto duration) -> auto {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    };

    auto testTotalDuration = [&](
        const std::string& name, const Options& options, int64_t numLoops, auto expectedPeriod) {
        genny::v1::RateLimiter limiter(options);

        auto maxDuration = numLoops * toMetricDuration(expectedPeriod);

        auto start = ClockT::now();

        auto volatile result = 0ll;
        for (auto i = 0ll; i < numLoops; ++i) {
            limiter.run([&] { ++result; });
        }
        auto end = ClockT::now();

        auto duration = toMetricDuration(end - start);
        REQUIRE(duration.count() < maxDuration.count());
        BOOST_LOG_TRIVIAL(info) << name << " test: " << numLoops << "ops/" << duration.count()
                                << "ns";

        REQUIRE(result == numLoops);
    };

    auto testPeriodDuration = [&](
        const std::string& name, const Options& options, int64_t numLoops, auto expectedPeriod) {
        genny::v1::RateLimiter limiter(options);

        // P90 isn't great, but apparently P95 doesn't always pass local machines
        auto periodThreshold = toMetricDuration(expectedPeriod) * 900 / 1000;

        auto last = genny::v1::RateLimiter::TimeT{};

        // Run once to start up the gap
        auto volatile result = 0ll;
        limiter.run([&] {
            ++result;

            last = ClockT::now();
        });
        for (auto i = 1ll; i < numLoops; ++i) {
            limiter.run([&] {
                ++result;

                auto now = ClockT::now();
                auto gap = toMetricDuration(now - last);
                last = now;
                REQUIRE(gap.count() > periodThreshold.count());
            });
        }

        REQUIRE(result == numLoops);
    };

    SECTION("Does not limit with default options") {
        testTotalDuration(
            "Default", Options{}, 100ll * 1000ll * 1000ll, std::chrono::nanoseconds(100ll));
    }

    SECTION("Limit with a few periods") {
        SECTION("1ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{1ll};

            auto constexpr kLogStr = "1ms period";
            auto constexpr kNumLoops = 1000ll;
            testTotalDuration(kLogStr, options, kNumLoops, options.minPeriod);
            testPeriodDuration(kLogStr, options, kNumLoops, options.minPeriod);
        }

        SECTION("10ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{10ll};

            auto constexpr kLogStr = "10ms period";
            auto constexpr kNumLoops = 100ll;
            testTotalDuration(kLogStr, options, kNumLoops, options.minPeriod);
            testPeriodDuration(kLogStr, options, kNumLoops, options.minPeriod);
        }

        SECTION("100ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{100ll};

            auto constexpr kLogStr = "100ms period";
            auto constexpr kNumLoops = 10ll;
            testTotalDuration(kLogStr, options, kNumLoops, options.minPeriod);
            testPeriodDuration(kLogStr, options, kNumLoops, options.minPeriod);
        }

        SECTION("999ms") {
            auto options = genny::v1::RateLimiter::Options{};
            options.minPeriod = std::chrono::milliseconds{999ll};

            auto constexpr kLogStr = "999ms period";
            auto constexpr kNumLoops = 2ll;
            testTotalDuration(kLogStr, options, kNumLoops, options.minPeriod);
            testPeriodDuration(kLogStr, options, kNumLoops, options.minPeriod);
        }
    }
}
