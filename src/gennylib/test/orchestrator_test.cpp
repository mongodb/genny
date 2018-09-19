#include "test.h"

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <boost/log/trivial.hpp>

#include <gennylib/Orchestrator.hpp>

using namespace genny;
using namespace std::chrono;

namespace {

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

TEST_CASE("Two non-blocking Phases") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> seen{};
    std::unordered_set<PhaseNumber> blocking;

    for (auto&& p : o.loop(blocking)) {
        seen.insert(p);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0, 1});
}

TEST_CASE("Single Blocking Phase") {
    Orchestrator o;
    o.addRequiredTokens(1);

    std::unordered_set<PhaseNumber> seen{};
    std::unordered_set<PhaseNumber> blocking{0};

    for (auto&& p : o.loop(blocking)) {
        seen.insert(p);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0});
}

TEST_CASE("single-threaded range-based for loops all phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    std::unordered_set<PhaseNumber> blocking{0, 1, 2};

    std::unordered_set<PhaseNumber> seen;

    for (PhaseNumber phase : o.loop(blocking)) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops no phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    std::unordered_set<PhaseNumber> blocking;  // none

    std::unordered_set<PhaseNumber> seen;

    for (auto phase : o.loop(blocking)) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops non-blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> blocking{1};

    std::unordered_set<PhaseNumber> seen;

    for (auto phase : o.loop(blocking)) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then non-blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> blocking{0};

    std::unordered_set<PhaseNumber> seen;

    for (auto phase : o.loop(blocking)) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_set<PhaseNumber> blocking{0, 1};

    std::unordered_set<PhaseNumber> seen;

    for (auto phase : o.loop(blocking)) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("Multi-threaded Range-based for loops") {
    Orchestrator o;
    o.addRequiredTokens(2);
    o.phasesAtLeastTo(1);

    const duration sleepTime = milliseconds{50};

    std::atomic_int failures = 0;

    // have we completed the sleep in phase 0?
    std::atomic_bool phaseZeroSlept = false;

    // have we completed the sleep in phase 1?
    std::atomic_bool phaseOneSlept = false;

    auto t1 = std::thread([&]() {
        std::unordered_set<PhaseNumber> blocking{1};
        for (int phase : o.loop(blocking)) {
            switch (phase) {
                case 0:
                    while (o.currentPhase() == 0) {}
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
                    BOOST_LOG_TRIVIAL(error) << "Unknown phase " << phase;
                    ++failures;
            }
        }
    });
    // similar to t1 but swapped zeroes and ones
    auto t2 = std::thread([&]() {
        std::unordered_set<PhaseNumber> blocking{0};

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (int phase : o.loop(blocking)) {
            switch (phase) {
                case 0:
                    std::this_thread::sleep_for(sleepTime);
                    phaseZeroSlept = true;
                    break;
                case 1:
                    while (o.currentPhase() == 1) {}
                    if (!phaseOneSlept) {
                        BOOST_LOG_TRIVIAL(error) << "Prematurely advanced from phase 1";
                        ++failures;
                    }
                    break;
                default:
                    BOOST_LOG_TRIVIAL(error) << "Unknown phase " << phase;
                    ++failures;
            }
        }
    });

    t1.join();
    t2.join();

    REQUIRE(failures == 0);
}
