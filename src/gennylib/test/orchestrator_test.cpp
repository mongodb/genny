#include "test.h"

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <thread>

#include <boost/log/trivial.hpp>

#include <gennylib/PhaseLoop.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/Actor.hpp>
#include <gennylib/context.hpp>

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

static int cnt = 0;

namespace genny {

struct IncrementsTwoRefs : public Actor {

    struct IncrPhaseConfig {
        int value;
        IncrPhaseConfig(const std::unique_ptr<PhaseContext>& ctx)
        : value{++cnt} {}
    };

    PhaseLoop<IncrPhaseConfig> _loop;
    int&phaseZero;
    int&phaseOne;

    IncrementsTwoRefs(ActorContext& actorContext, int&phaseZero, int&phaseOne)
    : _loop{actorContext},
      phaseZero{phaseZero},
      phaseOne{phaseOne} {}

    void run() override {
        for(auto&& [num, cfg] : _loop) {
            int iter = 0;
            for(auto&& _ : cfg) {
                if (num == 0) { ++phaseZero; }
                else if (num == 1 ) { ++phaseOne; }
                else { throw InvalidConfigurationException("what?"); }
            }
        }
    }

    static auto producer(int& phaseZero, int& phaseOne) {
        return [&](ActorContext& actorContext) {
            auto out = std::vector<std::unique_ptr<genny::Actor>>{};
            out.push_back(std::make_unique<IncrementsTwoRefs>(actorContext, phaseZero, phaseOne));
            return out;
        };
    }
};

}

// TODO: move to PhaseLoop test
TEST_CASE("Actual Actor Example", "[real]") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(0);  // we don't set this; rely on actor to do so

    metrics::Registry reg;

    YAML::Node doc = YAML::Load(R"(
SchemaVersion: 2018-07-01
MongoUri: mongodb://localhost:27017
Actors:
- Phases:
  - Phase: 0
    Repeat: 100
  - Phase: 1
    Repeat: 3
)");

    int phaseZeroCalls = 0;
    int phaseOneCalls =  0;

    std::vector<ActorProducer> producers = {IncrementsTwoRefs::producer(phaseZeroCalls, phaseOneCalls)};
    WorkloadContext wl {doc, reg, o, producers};

    wl.actors()[0]->run();

    REQUIRE(phaseZeroCalls == 100);
    REQUIRE(phaseOneCalls  == 3);
}

TEST_CASE("Two non-blocking Phases") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking{};
    blocking.try_emplace(0, o, std::make_unique<int>(7), std::make_optional(2), std::nullopt);
    blocking.try_emplace(1, o, std::make_unique<int>(9), std::make_optional(2), std::nullopt);

    std::unordered_set<PhaseNumber> seenPhases{};
    std::unordered_set<int> seenActorPhaseValues;

    auto count = 0;
    for (auto&& [p, h] : PhaseLoop<int>{o, std::move(blocking)}) {
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
    //    std::unordered_set<PhaseNumber> blocking{0};
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;


    for (auto&& [p, h] : PhaseLoop<int>{o, std::move(blocking)}) {
        seen.insert(p);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0});
}

TEST_CASE("single-threaded range-based for loops all phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    //    std::unordered_set<PhaseNumber> blocking{0, 1, 2};
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops no phases blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(2);

    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;  // none

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L, 2L});
}

TEST_CASE("single-threaded range-based for loops non-blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    //    std::unordered_set<PhaseNumber> blocking{1};
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then non-blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    //    std::unordered_set<PhaseNumber> blocking{0};
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
        seen.insert(phase);
    }

    REQUIRE(seen == std::unordered_set<PhaseNumber>{0L, 1L});
}

TEST_CASE("single-threaded range-based for loops blocking then blocking") {
    Orchestrator o;
    o.addRequiredTokens(1);
    o.phasesAtLeastTo(1);

    //    std::unordered_set<PhaseNumber> blocking{0, 1};
    std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

    std::unordered_set<PhaseNumber> seen;

    for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
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
        //        std::unordered_set<PhaseNumber> blocking{1};
        std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
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
        //        std::unordered_set<PhaseNumber> blocking{0};
        std::unordered_map<PhaseNumber, V1::ActorPhase<int>> blocking;

        auto prevPhaseStart = system_clock::now();
        int prevPhase = -1;

        for (auto&& [phase, holder] : PhaseLoop<int>{o, std::move(blocking)}) {
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
