#include "test.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <boost/log/trivial.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

using namespace genny;
using namespace std::chrono;
using namespace std;

namespace {

//
// Cute convenience operators -
//  100_i   gives optional<int>     holding 100
//  100_ms  gives optional<millis>  holding 100
//
optional<int> operator"" _i(unsigned long long int v) {
    return make_optional(v);
}
optional<chrono::milliseconds> operator"" _ms(unsigned long long int v) {
    return make_optional(chrono::milliseconds{v});
}


std::thread start(Orchestrator& o,
                  PhaseNumber phase,
                  const bool block = true,
                  const int addTokens = 1) {
    return std::thread{[&o, phase, block, addTokens]() {
        REQUIRE(o.awaitPhaseStart(block, addTokens) == phase);
        REQUIRE(o.currentPhase() == phase);
    }};
}

std::thread end(Orchestrator& o,
                PhaseNumber phase,
                const bool block = true,
                const int removeTokens = 1) {
    return std::thread{[&o, phase, block, removeTokens]() {
        REQUIRE(o.currentPhase() == phase);
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
    auto o = Orchestrator{};
    o.addRequiredTokens(2);

    // 2 tokens but we only count down 1 so normally would block
    auto t1 = start(o, 0, false, 1);
    t1.join();
}

TEST_CASE("Non-Blocking end (background progression)") {
    auto o = Orchestrator{};
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
    auto o = Orchestrator{};
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
    auto o = Orchestrator{};
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
    auto o = Orchestrator{};
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

// more easily construct V1::ActorPhase instances
using PhaseConfig =
    std::tuple<PhaseNumber, int, std::optional<int>, std::optional<std::chrono::milliseconds>>;
std::unordered_map<PhaseNumber, V1::ActorPhase<int>> makePhaseConfig(
    Orchestrator& orchestrator, const std::vector<PhaseConfig>& phaseConfigs) {
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> out;
    for (auto&& [phaseNum, phaseVal, iters, dur] : phaseConfigs) {
        out.try_emplace(
            phaseNum, orchestrator, make_unique<int>(phaseVal), V1::IterationCompletionCheck{iters, dur});
    }
    return out;
};

TEST_CASE("Two non-blocking Phases") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> seenPhases{};
    std::unordered_set<int> seenActorPhaseValues;

    auto phaseConfig{makePhaseConfig(o, {{0, 7, 2_i, nullopt}, {1, 9, 2_i, nullopt}})};

    auto count = 0;
    for (auto&& [p, h] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seenPhases.insert(p);
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
    Orchestrator o;
    o.addRequiredTokens(1);

    std::unordered_set<PhaseNumber> seen{};

    auto phaseConfig{makePhaseConfig(o, {{0, 7, 1_i, nullopt}})};
    for (auto&& [p, h] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(p);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0});
}

TEST_CASE("single-threaded range-based for loops all phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    auto phaseConfig{makePhaseConfig(o,
                                     {// all blocking on # iterations
                                      {0, 7, 1_i, nullopt},
                                      {1, 9, 2_i, nullopt},
                                      {2, 11, 3_i, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops no phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    auto phaseConfig{makePhaseConfig(o,
                                     {// none blocking
                                      {0, 7, nullopt, nullopt},
                                      {1, 9, nullopt, nullopt},
                                      {2, 11, nullopt, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops non-blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// non-block then block
                                      {0, 7, nullopt, nullopt},
                                      {1, 9, 1_i, nullopt}})};
    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then non-blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// block then non-block
                                      {0, 7, 1_i, nullopt},
                                      {1, 9, nullopt, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    auto phaseConfig{makePhaseConfig(o,
                                     {// block then block
                                      {0, 7, 1_i, nullopt},
                                      {1, 9, 1_i, nullopt}})};

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("Multi-threaded Range-based for loops") {
    Orchestrator o;
    o.addRequiredTokens(2);
    o.phasesAtLeastTo(1);

    std::unordered_map<int, system_clock::duration> t1TimePerPhase;
    std::unordered_map<int, system_clock::duration> t2TimePerPhase;

    const duration sleepTime = milliseconds{100};

    std::atomic_int failures = 0;

    auto t1 = std::thread([&]() {
        auto phaseConfig{makePhaseConfig(o,
                                         {// non-block then block
                                          {0, 7, nullopt, nullopt},
                                          {1, 9, 1_i, nullopt}})};

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
            if (!(phase == 1 || phase == 0)) {
                ++failures;
            }

            if (prevPhase != -1) {
                auto now = system_clock::now();
                t1TimePerPhase[prevPhase] = now - prevPhaseStart;
                prevPhaseStart = now;
            }
            prevPhase = phase;

            if (phase == 1) {
                // blocks t2 from progressing
                std::this_thread::sleep_for(sleepTime);
            }

            if (phase == 0) {
                while (o.currentPhase() == 0) {
                    // nop
                }
            }
        }
        if (prevPhase != 1) {
            ++failures;
        }
        t1TimePerPhase[prevPhase] = system_clock::now() - prevPhaseStart;
    });
    auto t2 = std::thread([&]() {
        auto phaseConfig{makePhaseConfig(o,
                                         {// non-block then block
                                          {0, 7, 1_i, nullopt},
                                          {1, 9, nullopt, nullopt}})};

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(phaseConfig)}) {
            if (!(phase == 1 || phase == 0)) {
                ++failures;
            }

            if (prevPhase != -1) {
                auto now = system_clock::now();
                t2TimePerPhase[prevPhase] = now - prevPhaseStart;
                prevPhaseStart = now;
            }
            prevPhase = phase;

            if (phase == 1) {
                while (o.currentPhase() == 1) {
                    // nop
                }
            }

            if (phase == 0) {
                // blocks t1 from progressing
                std::this_thread::sleep_for(sleepTime);
            }
        }
        if (prevPhase != 1) {
            ++failures;
        }
        t2TimePerPhase[prevPhase] = system_clock::now() - prevPhaseStart;
    });

    t1.join();
    t2.join();

    // don't care about 1s place, so just int-divide to kill it :)
    REQUIRE(duration_cast<milliseconds>(t1TimePerPhase[0]).count() / 10 == sleepTime.count() / 10);
    REQUIRE(duration_cast<milliseconds>(t1TimePerPhase[1]).count() / 10 == sleepTime.count() / 10);
    REQUIRE(duration_cast<milliseconds>(t2TimePerPhase[0]).count() / 10 == sleepTime.count() / 10);
    REQUIRE(duration_cast<milliseconds>(t2TimePerPhase[1]).count() / 10 == sleepTime.count() / 10);
}
