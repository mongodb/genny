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

#include <testlib/helpers.hpp>

#include <limits>
#include <vector>

#include <boost/log/trivial.hpp>

#include <canaries/Loops.hpp>

namespace genny::testing {
namespace {
using namespace genny::canaries;

/**
 * Measure the overhead of various loops. Each loop is configured to run on
 * the scale of 1 second.
 *
 * If you're running this benchmark locally, please ensure there are no other
 * programs. The CPU cache benchmarks are sensitive to context switching overhead.
 */
TEST_CASE("Measure Phaseloop Overhead", "[benchmark]") {
    std::vector<std::string> loopNames{"simple", "metrics", "metrics-ftdc", "phase", "real", "real-ftdc"};

    auto printRes = [&](std::vector<Nanosecond>& loopTimings, std::string_view name) {
        BOOST_LOG_TRIVIAL(info) << "Total duration for " << name << ":";
        for (int i = 0; i < loopTimings.size(); i++) {
            BOOST_LOG_TRIVIAL(info)
                << std::setw(8) << loopNames[i] << ": " << loopTimings[i] << "ns";
        }
    };

    /**
     * Check that the timing of each loop is less than 2% slower than the previous loop
     *
     * The ordering of the loop is defined in "loopNames", from fastest to slowest.
     */
    auto validateTimingRange = [&](std::vector<Nanosecond>& loopTimings, std::string_view name) {
        printRes(loopTimings, name);

        // The threshold is (difference * 1), which is 100%.
        const int threshold = 1;

        auto s = loopTimings[0];
        auto m = loopTimings[1];
        auto p = loopTimings[2];
        auto r = loopTimings[3];

        // Write out all the requires explicitly so it's easy to find the line number.

        // Compare phase and metrics loops with native loops.
        REQUIRE((m - s) * threshold < m);
        REQUIRE((p - s) * threshold < p);

        // Compare real loops with phase loops.
        REQUIRE((r - p) * threshold < r);
        REQUIRE((r - p) * threshold < r);

        // Compare real loops with metrics loops.
        REQUIRE((r - m) * threshold < r);
        REQUIRE((r - m) * threshold < r);
    };

    SECTION("nop") {
        // Run NopTask 1 million times. The total time was ~40ms for the "phase" loop
        // version and ~200ms for the "real" loop. Replace "ms" with "ns" to get the
        // average time per loop.
        auto nopRes = runTest<NopTask>(loopNames, 1e6);

        // Don't validate the results of nop loops, just print the results.
        // The results are going to be wildly different between the different loops.
        printRes(nopRes, "nop");

        // Do a simple assert that the basic loop (0th index) is within some
        // (wide) range as a sanity check that it's not being optimized out
        // or the machine is broken.
        REQUIRE(nopRes[0] > 1e6);       // Each iteration can't take less than 1ns.
        REQUIRE(nopRes[0] < 50 * 1e6);  // Each iteration can't take more than 50ns.
    }

    SECTION("sleep") {
        // Run sleep 1ms for 100 iterations. Total time ~130ms.
        auto sleepRes = runTest<SleepTask>(loopNames, 100);
        validateTimingRange(sleepRes, "sleep");
    }

    SECTION("cpu") {
        // Run CPU task for 10k iterations. Total time a few hundred ms.
        auto cpuRes = runTest<CPUTask>(loopNames, 1e4);
        validateTimingRange(cpuRes, "cpu");
    }

    SECTION("l2") {
        // Run the L2 task for 10k iterations. Total time a few hundred ms.
        auto l2Res = runTest<L2Task>(loopNames, 1e4);
        validateTimingRange(l2Res, "l2");
    }

    SECTION("l3") {
        // Run the L3 task for 100 iterations. This can take ~100ms to a
        // second depending on if you have 8MB of L3 cache available
        // to this program or not.
        auto l3Res = runTest<L3Task>(loopNames, 100);
        validateTimingRange(l3Res, "l3");
    }
}
}  // namespace
}  // namespace genny::testing
