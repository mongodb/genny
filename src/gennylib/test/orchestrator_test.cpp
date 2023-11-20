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

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <boost/log/trivial.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <testlib/helpers.hpp>

using namespace genny;
using namespace std::chrono;
using namespace std;

namespace {

// Catch2's REQUIRE etc macros are not thread-safe, so need to unique_lock on this
// mutex whenever calling assertion macros inside a thread.
std::mutex asserting;

//
// Cute convenience operators -
//  100_uis   gives optional<IntegerSpec>     holding 100
//  100_ts  gives optional<millis>  holding 100
//
// These are copy/pasta in PhaseLoop_test and orchestrator_test. Refactor.
optional<IntegerSpec> operator""_uis(unsigned long long v) {
    return make_optional(IntegerSpec(v));
}
optional<TimeSpec> operator""_ots(unsigned long long v) {
    return make_optional(TimeSpec(chrono::milliseconds{v}));
}
TimeSpec operator""_ts(unsigned long long v) {
    return TimeSpec(chrono::milliseconds{v});
}

std::thread start(Orchestrator& o,
                  PhaseNumber phase,
                  const bool block = true,
                  const int addTokens = 1) {
    return std::thread{[&o, phase, block, addTokens]() {
        auto result = o.awaitPhaseStart(block, addTokens);
        {
            std::unique_lock<std::mutex> lk(asserting);
            REQUIRE(result == phase);
            REQUIRE(o.currentPhase() == phase);
        }
    }};
}

std::thread end(Orchestrator& o,
                PhaseNumber phase,
                const bool block = true,
                const int removeTokens = 1) {
    return std::thread{[&o, phase, block, removeTokens]() {
        auto current = o.currentPhase();
        {
            std::unique_lock<std::mutex> lk(asserting);
            REQUIRE(current == phase);
        }
        o.awaitPhaseEnd(block, removeTokens);
    }};
}

bool advancePhase(Orchestrator& o) {
    bool out = true;
    auto t = std::thread([&]() {
        o.awaitPhaseStart();
        out = o.awaitPhaseEnd();
    });
    t.join();
    return out;
}

}  // namespace

TEST_CASE("Non-Blocking start") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);

    // 2 tokens but we only count down 1 so normally would block
    auto t1 = start(o, 0, false, 1);
    t1.join();
}

// Timing tests are ignored on MacOS, because we only target Linux for performance tests
TEST_CASE("Non-Blocking end (background progression), IgnoredOnMacOs") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);

    auto bgIters = 0;
    auto fgIters = 0;

    auto t1 = std::thread([&]() {
        auto phase = o.awaitPhaseStart();
        o.awaitPhaseEnd(false);
        while (phase == o.currentPhase()) {
            ++bgIters;
            std::this_thread::sleep_for(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds{1}));
        }
    });
    auto t2 = std::thread([&]() {
        o.awaitPhaseStart();
        std::this_thread::sleep_for(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds{5}));
        ++fgIters;
        o.awaitPhaseEnd();
    });
    t1.join();
    t2.join();

    REQUIRE(bgIters >= 4);
    REQUIRE(bgIters <= 6);

    REQUIRE(fgIters == 1);
}

TEST_CASE("Can add more tokens at start") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);

    auto t1 = start(o, 0, false, 2);
    t1.join();

    REQUIRE(o.currentPhase() == 0);

    auto t2 = end(o, 0);
    auto t3 = end(o, 0);
    t2.join();
    t3.join();

    REQUIRE(o.currentPhase() == 1);
}


TEST_CASE("Set minimum number of phases") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    REQUIRE(o.currentPhase() == 0);
    o.phasesAtLeastTo(1);
    REQUIRE(advancePhase(o));  // 0->1

    REQUIRE(o.currentPhase() == 1);
    REQUIRE(o.morePhases());
    REQUIRE(!advancePhase(o));  // 1->2

    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhase() == 2);

    o.phasesAtLeastTo(0);  // effectively nop, can't set lower than what it currently is
    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhase() == 2);  // still

    o.phasesAtLeastTo(2);
    REQUIRE(o.morePhases());

    REQUIRE(!advancePhase(o));  // 2->3
    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhase() == 3);
}

TEST_CASE("Orchestrator") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);
    o.phasesAtLeastTo(1);

    REQUIRE(o.currentPhase() == 0);
    REQUIRE(o.morePhases());

    auto t1 = start(o, 0);
    auto t2 = start(o, 0);
    t1.join();
    t2.join();

    REQUIRE(o.currentPhase() == 0);
    REQUIRE(o.morePhases());

    auto t3 = end(o, 0);
    auto t4 = end(o, 0);
    t3.join();
    t4.join();

    REQUIRE(o.currentPhase() == 1);
    REQUIRE(o.morePhases());

    // now all wait for phase 1

    auto t5 = start(o, 1);
    auto t6 = start(o, 1);
    t5.join();
    t6.join();

    REQUIRE(o.currentPhase() == 1);
    REQUIRE(o.morePhases());

    SECTION("Default has phases 0 and 1") {
        auto t7 = end(o, 1);
        auto t8 = end(o, 1);
        t7.join();
        t8.join();

        REQUIRE(o.currentPhase() == 2);
        REQUIRE(!o.morePhases());
    }

    SECTION("Can add more phases") {
        auto t7 = end(o, 1, true, 1);
        auto t8 = end(o, 1, true, 1);
        o.phasesAtLeastTo(2);
        t7.join();
        t8.join();
        REQUIRE(o.currentPhase() == 2);
        REQUIRE(o.morePhases());
    }
}

// more easily construct v1::ActorPhase instances
using PhaseConfig =
    std::tuple<PhaseNumber, int, std::optional<IntegerSpec>, std::optional<TimeSpec>>;

std::unordered_map<PhaseNumber, v1::ActorPhase<int>> makePhaseConfig(
    Orchestrator& orchestrator, const std::vector<PhaseConfig>& phaseConfigs) {

    std::unordered_map<PhaseNumber, v1::ActorPhase<int>> out;
    for (auto&& [phaseNum, phaseVal, iters, dur] : phaseConfigs) {
        auto [it, success] = out.try_emplace(
            phaseNum,
            orchestrator,
            std::make_unique<v1::IterationChecker>(dur, iters, false, 0_ts, 0_ts, nullopt),
            phaseNum,
            phaseVal);
        // prevent misconfiguration within test (dupe phaseNum vals)
        {
            std::unique_lock<mutex> lk(asserting);
            REQUIRE(success);
            REQUIRE(*(it->second) == phaseVal);
        }
    }
    return out;
};

TEST_CASE("Two non-blocking Phases") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> seenPhases{};
    std::unordered_set<int> seenActorPhaseValues;

    auto phaseConfig{makePhaseConfig(o, {{0, 7, 2_uis, nullopt}, {1, 9, 2_uis, nullopt}})};

    auto count = 0;
    for (auto&& h : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seenPhases.insert(h.phaseNumber());
        for (auto&& _ : h) {
            seenActorPhaseValues.insert(*h);
            ++count;
        }
    }

    REQUIRE(count == 4);
    REQUIRE(seenPhases == std::unordered_set<PhaseNumber>{0, 1});
    REQUIRE(seenActorPhaseValues == std::unordered_set<int>{7, 9});
}

TEST_CASE("Single Blocking Phase") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);

    std::unordered_set<PhaseNumber> seen{};

    auto phaseConfig{makePhaseConfig(o, {{0, 7, 1_uis, nullopt}})};
    for (auto&& h : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(h.phaseNumber());
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0});
}

TEST_CASE("single-threaded range-based for loops all phases blocking") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    auto phaseConfig{makePhaseConfig(o,
                                     {// all blocking on # iterations
                                      {0, 7, 1_uis, nullopt},
                                      {1, 9, 2_uis, nullopt},
                                      {2, 11, 3_uis, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    auto iters = 0;

    for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(holder.phaseNumber());
        for (auto&& _ : holder) {
            ++iters;
        }
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
    REQUIRE(iters == 6);
}

TEST_CASE("single-threaded range-based for loops no phases blocking") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    auto phaseConfig{makePhaseConfig(o,
                                     {// none blocking
                                      {0, 7, nullopt, nullopt},
                                      {1, 9, nullopt, nullopt},
                                      {2, 11, nullopt, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    auto iters = 0;

    for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(holder.phaseNumber());
        for (auto&& _ : holder) {
            ++iters;
        }
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
    REQUIRE(iters == 0);  // all non-blocking
}

TEST_CASE("single-threaded range-based for loops non-blocking then blocking") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// non-block then block
                                      {0, 7, nullopt, nullopt},
                                      {1, 9, 1_uis, nullopt}})};
    std::unordered_set<PhaseNumber> seen;

    auto iters = 0;
    for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(holder.phaseNumber());
        for (auto&& _ : holder) {
            ++iters;
        }
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
    REQUIRE(iters == 1);
}

TEST_CASE("single-threaded range-based for loops blocking then non-blocking") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// block then non-block
                                      {0, 7, 1_uis, nullopt},
                                      {1, 9, nullopt, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    auto iters = 0;

    for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(holder.phaseNumber());
        for (auto&& _ : holder) {
            ++iters;
        }
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
    REQUIRE(iters == 1);
}

TEST_CASE("single-threaded range-based for loops blocking then blocking") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// block then block
                                      {0, 7, 1_uis, nullopt},
                                      {1, 9, 1_uis, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    auto iters = 0;

    for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(holder.phaseNumber());
        for (auto&& _ : holder) {
            ++iters;
        }
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
    REQUIRE(iters == 2);
}

// Timing tests are ignored on MacOS, because we only target Linux for performance tests
TEST_CASE("Range-based for stops when Orchestrator says Phase is done, IgnoredOnMacOs") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);

    std::atomic_bool blockingDone = false;

    auto start = std::chrono::steady_clock::now();

    // t1 blocks for 75ms in Phase 0
    auto t1 = std::thread([&]() {
        for (auto&& h : PhaseLoop<int>{o, makePhaseConfig(o, {{0, 0, nullopt, 75_ots}})})
            for (auto _ : h) {
            }  // nop
        blockingDone = true;
    });

    // t2 does not block
    auto t2 = std::thread([&]() {
        for (auto&& h : PhaseLoop<int>{o, makePhaseConfig(o, {{0, 0, nullopt, nullopt}})})
            for (auto _ : h) {
            }  // nop
        {
            std::lock_guard lk{asserting};
            REQUIRE(blockingDone);
        }
    });

    t1.join();
    t2.join();

    // test of the test kinda: we should have blocked at least as long as t1
    REQUIRE(std::chrono::steady_clock::now() - start >= chrono::milliseconds{75});
}

TEST_CASE("Multi-threaded Range-based for loops") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);
    o.phasesAtLeastTo(1);

    const duration sleepTime = milliseconds{50};

    std::atomic_int failures = 0;

    // have we completed the sleep in phase 0?
    std::atomic_bool phaseZeroSlept = false;

    // have we completed the sleep in phase 1?
    std::atomic_bool phaseOneSlept = false;

    auto t1 = std::thread([&]() {
        auto phaseConfig{makePhaseConfig(o,
                                         {// non-block then block
                                          {0, 7, nullopt, nullopt},
                                          {1, 9, 1_uis, nullopt}})};

        for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
            switch (holder.phaseNumber()) {
                case 0:
                    for (auto&& _ : holder) {
                    }  // nop
                    // is set after nop
                    if (!phaseZeroSlept) {
                        BOOST_LOG_TRIVIAL(error) << "Prematurely advanced from phase 0";
                        ++failures;
                    }
                    break;
                case 1:
                    std::this_thread::sleep_for(sleepTime);
                    phaseOneSlept = true;
                    break;
                default:
                    BOOST_LOG_TRIVIAL(error) << "Unknown phase " << holder.phaseNumber();
                    ++failures;
            }
        }
    });
    // similar to t1 but swapped zeroes and ones
    auto t2 = std::thread([&]() {
        auto phaseConfig{makePhaseConfig(o,
                                         {// block then non-block
                                          {0, 7, 1_uis, nullopt},
                                          {1, 9, nullopt, nullopt}})};

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (auto&& holder : PhaseLoop<int>{o, std::move(phaseConfig)}) {
            switch (holder.phaseNumber()) {
                case 0:
                    std::this_thread::sleep_for(sleepTime);
                    phaseZeroSlept = true;
                    break;
                case 1:
                    for (auto&& _ : holder) {
                    }  // nop
                    if (!phaseOneSlept) {
                        BOOST_LOG_TRIVIAL(error) << "Prematurely advanced from phase 1";
                        ++failures;
                    }
                    break;
                default:
                    BOOST_LOG_TRIVIAL(error) << "Unknown phase " << holder.phaseNumber();
                    ++failures;
            }
        }
    });

    t1.join();
    t2.join();

    REQUIRE(failures == 0);
}
