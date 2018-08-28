#include "test.h"

#include <iostream>

#include <gennylib/Orchestrator.hpp>

using namespace genny;


namespace {

std::thread start(Orchestrator& o, const int phase, const bool block=true, const int addTokens=1) {
    return std::thread{[&o, phase, block, addTokens]() {
        REQUIRE(o.awaitPhaseStart(block, addTokens) == phase);
        REQUIRE(o.currentPhaseNumber() == phase);
    }};
}

std::thread end(Orchestrator& o, const int phase, const bool block=true, const unsigned int morePhases=0, const int removeTokens=1) {
    return std::thread{[&o, phase, block, morePhases, removeTokens]() {
        REQUIRE(o.currentPhaseNumber() == phase);
        o.awaitPhaseEnd(block, morePhases, removeTokens);
    }};
}

}  // namespace

TEST_CASE("Non-Blocking Orchestration") {
    auto o = Orchestrator{};
    o.addTokens(2);

    // 2 tokens but we only count down 1 so normally would block
    auto t1 = start(o, 0, false, 1);
    t1.join();
}

TEST_CASE("Add more tokens at start") {
    auto o = Orchestrator{};
    o.addTokens(2);

    auto t1 = start(o, 0, false, 2);
    t1.join();

    auto t2 = end(o, 0, true, 0, 1);
    auto t3 = end(o, 0, true, 0, 1);
    t2.join();
    t3.join();

    REQUIRE(o.currentPhaseNumber() == 1);
}


TEST_CASE("Orchestrator") {
    auto o = Orchestrator{};
    o.addTokens(2);

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

    auto t7 = end(o, 1);
    auto t8 = end(o, 1);
    t7.join();
    t8.join();

    REQUIRE(o.currentPhaseNumber() == 2);
    REQUIRE(!o.morePhases());
}
