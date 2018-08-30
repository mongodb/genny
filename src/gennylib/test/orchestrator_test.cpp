#include "test.h"

#include <chrono>
#include <iostream>

#include <gennylib/Orchestrator.hpp>

using namespace genny;
using namespace std::chrono;

namespace {

std::thread start(Orchestrator& o,
                  const int phase,
                  const bool block = true,
                  const int addTokens = 1) {
    return std::thread{[&o, phase, block, addTokens]() {
        REQUIRE(o.awaitPhaseStart(block, addTokens) == phase);
        REQUIRE(o.currentPhaseNumber() == phase);
    }};
}

std::thread end(Orchestrator& o,
                const int phase,
                const bool block = true,
                const int removeTokens = 1) {
    return std::thread{[&o, phase, block, removeTokens]() {
        REQUIRE(o.currentPhaseNumber() == phase);
        o.awaitPhaseEnd(block, removeTokens);
    }};
}

bool advancePhase(Orchestrator& o) {
    bool out = true;
    auto t = std::thread([&](){
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
        while (phase == o.currentPhaseNumber()) {
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

    REQUIRE(o.currentPhaseNumber() == 0);

    auto t2 = end(o, 0);
    auto t3 = end(o, 0);
    t2.join();
    t3.join();

    REQUIRE(o.currentPhaseNumber() == 1);
}


TEST_CASE("Set minimum number of phases") {
    auto o = Orchestrator{};
    REQUIRE(o.currentPhaseNumber() == 0);
    o.phasesAtLeastTo(1);
    REQUIRE(advancePhase(o)); // 0->1

    REQUIRE(o.currentPhaseNumber() == 1);
    REQUIRE(o.morePhases());
    REQUIRE(!advancePhase(o)); //1->2

    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhaseNumber() == 2);

    o.phasesAtLeastTo(0); // effectively nop, can't set lower than what it currently is
    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhaseNumber() == 2); // still

    o.phasesAtLeastTo(2);
    REQUIRE(o.morePhases());

    REQUIRE(!advancePhase(o)); // 2->3
    REQUIRE(!o.morePhases());
    REQUIRE(o.currentPhaseNumber() == 3);
}

TEST_CASE("Orchestrator") {
    auto o = Orchestrator{};
    o.addRequiredTokens(2);

    REQUIRE(o.currentPhaseNumber() == 0);
    REQUIRE(o.morePhases());

    auto t1 = start(o, 0);
    auto t2 = start(o, 0);
    t1.join();
    t2.join();

    REQUIRE(o.currentPhaseNumber() == 0);
    REQUIRE(o.morePhases());

    auto t3 = end(o, 0);
    auto t4 = end(o, 0);
    t3.join();
    t4.join();

    REQUIRE(o.currentPhaseNumber() == 1);
    REQUIRE(o.morePhases());

    // now all wait for phase 1

    auto t5 = start(o, 1);
    auto t6 = start(o, 1);
    t5.join();
    t6.join();

    REQUIRE(o.currentPhaseNumber() == 1);
    REQUIRE(o.morePhases());

    SECTION("Default has phases 0 and 1") {
        auto t7 = end(o, 1);
        auto t8 = end(o, 1);
        t7.join();
        t8.join();

        REQUIRE(o.currentPhaseNumber() == 2);
        REQUIRE(!o.morePhases());
    }

    SECTION("Can add more phases") {
        auto t7 = end(o, 1, true, 1);
        auto t8 = end(o, 1, true, 1);
        o.phasesAtLeastTo(2);
        t7.join();
        t8.join();
        REQUIRE(o.currentPhaseNumber() == 2);
        REQUIRE(o.morePhases());
    }
}

TEST_CASE("Range-based for loops") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_map<int, system_clock::duration> t1TimePerPhase;
    std::unordered_map<int, system_clock::duration> t2TimePerPhase;

    const duration sleepTime = milliseconds{10};

    std::atomic_int failures = 0;

    auto t1 = std::thread([&]() {
        std::unordered_map<long, bool> blocking = {
                {0, false},
                {1, true}
        };

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for(int phase : o.loop(blocking)) {
            if(!(phase == 1 || phase == 0)) {
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
        }
        if (prevPhase != 1) {
            ++failures;
        }
        t1TimePerPhase[prevPhase] = system_clock::now() - prevPhaseStart;
    });
    auto t2 = std::thread([&]() {
        std::unordered_map<long, bool> blocking = {
                {0, true},
                {1, false}
        };

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for(int phase : o.loop(blocking)) {
            if(!(phase == 1 || phase == 0)) {
                ++failures;
            }

            if (prevPhase != -1) {
                auto now = system_clock::now();
                t2TimePerPhase[prevPhase] = now - prevPhaseStart;
                prevPhaseStart = now;
            }
            prevPhase = phase;

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

    REQUIRE(duration_cast<milliseconds>(t1TimePerPhase[0]).count() == sleepTime.count());
    REQUIRE(duration_cast<milliseconds>(t1TimePerPhase[1]).count() == sleepTime.count());
//    REQUIRE(t2TimePerPhase[0] == sleepTime);
//    REQUIRE(t2TimePerPhase[1] == sleepTime);
}
