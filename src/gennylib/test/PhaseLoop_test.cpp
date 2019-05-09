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
#include <optional>

#include "NopActor.hpp"
#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <testlib/ActorHelper.hpp>

#include <testlib/clocks.hpp>
#include <testlib/helpers.hpp>

using namespace genny;
using namespace genny::v1;
using namespace std;

namespace {

//
// Cute convenience operators -
//  100_uis   gives optional<IntegerSpec> holding 100
//  100_ts  gives optional<TimeSpec>   holding 100
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

}  // namespace

TEST_CASE("Correctness for N iterations") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};

    SECTION("Loops 0 Times") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(nullopt, 0_uis, false, 0_ts, 0_ts, nullopt),
            1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 0);
    }
    SECTION("Loops 1 TimeSpec") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(nullopt, 1_uis, false, 0_ts, 0_ts, nullopt),
            1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 1);
    }
    SECTION("Loops 113 Times") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(nullopt, 113_uis, false, 0_ts, 0_ts, nullopt),
            1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 113);
    }
}

TEST_CASE("Correctness for N milliseconds") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    SECTION("Loops 0 milliseconds so zero times") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(0_ots, nullopt, false, 0_ts, 0_ts, nullopt),
            0};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 0);
    }
    SECTION("Looping for 10 milliseconds takes between 10 and 11 milliseconds") {
        // we nop in the loop so ideally it should take exactly 10ms, but don't want spurious
        // failures
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(10_ots, nullopt, false, 0_ts, 0_ts, nullopt),
            0};

        auto start = chrono::system_clock::now();
        for (auto _ : loop) {
        }  // nop
        auto elapsed =
            chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start)
                .count();

        REQUIRE(elapsed >= 10);
        REQUIRE(elapsed <= 11);
    }
}

TEST_CASE("Combinations of duration and iterations") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    SECTION("Loops 0 milliseconds but 100 times") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(0_ots, 100_uis, false, 0_ts, 0_ts, nullopt),
            0};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 100);
    }
    SECTION("Loops 5 milliseconds, 100 times: 10 millis dominates") {
        v1::ActorPhase<int> loop{
            o,
            std::make_unique<v1::IterationChecker>(5_ots, 100_uis, false, 0_ts, 0_ts, nullopt),
            0};

        auto start = chrono::system_clock::now();
        int i = 0;
        for (auto _ : loop)
            ++i;
        auto elapsed =
            chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start)
                .count();

        REQUIRE(i > 100);
        REQUIRE(elapsed >= 5);
        REQUIRE(elapsed <= 6);
    }
    // It's tempting to write a test where the # iterations dominates the time
    // e.g. OperationLoop loop{1000000000000_uis, 1_ts} but that would fail spuriously
    // on super-fast hardware. So resist the temptation and trust the logical
    // combinations of the other tests 🙈

    SECTION("Configured for -1 milliseconds barfs") {
        REQUIRE_THROWS_WITH(
            (v1::ActorPhase<int>{
                o,
                std::make_unique<v1::IterationChecker>(
                    make_optional(TimeSpec{-1}), nullopt, false, 0_ts, 0_ts, nullopt),
                0}),
            Catch::Contains("Need non-negative duration. Gave -1 milliseconds"));
    }
}

TEST_CASE("Can do without either iterations or duration") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    v1::ActorPhase<int> actorPhase{
        o, std::make_unique<v1::IterationChecker>(nullopt, nullopt, false, 0_ts, 0_ts, nullopt), 0};
    auto iters = 0;
    for (auto&& _ : actorPhase) {
        ++iters;
        // we continue indefinitely; choose 500 as an arbitrarily large stopping point.
        if (iters >= 500) {
            break;
        }
    }
    REQUIRE(iters == 500);
    REQUIRE(o.currentPhase() == 0);
}

TEST_CASE("Iterator concept correctness") {
    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    v1::ActorPhase<int> loop{
        o, std::make_unique<v1::IterationChecker>(nullopt, 1_uis, false, 0_ts, 0_ts, nullopt), 0};

    // can deref
    SECTION("Deref and advance works") {
        auto it = loop.begin();
        REQUIRE(it != loop.end());
        *it;
        REQUIRE(it != loop.end());
        ++it;
        REQUIRE(it == loop.end());
        REQUIRE(it == loop.end());
        REQUIRE(loop.end() == it);

        auto end = loop.end();
        *end;
        ++end;
        REQUIRE(end == end);
        REQUIRE(end == loop.end());

        // can still advance and still deref
        // debatable about whether this *should* work or whether it should be tested/asserted
        ++it;
        *it;
        REQUIRE(it == loop.end());
    }

    SECTION("Equality is maintained through incrementation") {
        auto it1 = loop.begin();
        auto it2 = loop.begin();

        REQUIRE(it1 == it1);
        REQUIRE(it2 == it2);

        REQUIRE(it1 == it2);
        REQUIRE(it2 == it1);

        ++it1;
        REQUIRE(it1 != it2);
        REQUIRE(it2 != it1);

        ++it2;
        REQUIRE(it1 == it2);
        REQUIRE(it2 == it1);
    }

    SECTION("End iterators all equal") {
        auto end1 = loop.end();
        auto end2 = loop.end();
        REQUIRE(end1 == end2);
        REQUIRE(end2 == end1);
        REQUIRE(end1 == end1);
        REQUIRE(end2 == end2);
    }
}

template <typename ActorT>
struct CounterProducer : public ActorProducer {
    using ActorProducer::ActorProducer;

    ActorVector produce(ActorContext& actorContext) override {
        ActorVector out;
        out.emplace_back(std::make_unique<ActorT>(actorContext, counters));
        return out;
    }

    std::unordered_map<int, int> counters;
};

TEST_CASE("Actual Actor Example") {

    class IncrementsMapValues : public Actor {
    protected:
        struct IncrPhaseConfig {
            int _key;
            IncrPhaseConfig(PhaseContext& ctx, int keyOffset)
                : _key{ctx.get<int>("Key") + keyOffset} {}
        };

        PhaseLoop<IncrPhaseConfig> _loop;
        std::unordered_map<int, int>& _counters;

    public:
        IncrementsMapValues(ActorContext& actorContext, std::unordered_map<int, int>& counters)
            : Actor(actorContext), _loop{actorContext, 1}, _counters{counters} {}
        //                        ↑ is forwarded to the IncrementsMapValues ctor as the keyOffset
        //                        param.

        void run() override {
            for (auto&& cfg : _loop) {
                for (auto&& _ : cfg) {
                    ++this->_counters[cfg->_key];
                }
            }
        }
    };

    SECTION("Simple Actor") {
        // ////////
        // setup and run (bypass the driver)
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Repeat: 100
                Key: 71
              - Repeat: 3
                Key: 93
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");
        ActorHelper ah(config, 1, {{"Inc", imvProducer}});
        ah.run();

        REQUIRE(imvProducer->counters ==
                std::unordered_map<int, int>{
                    {72, 100},  // keys & vals came from yaml config. Keys have a +1 offset.
                    {94, 3}});
    }

    /**
     * Tests an actor with a Nop command. See YAML Node below.
     */
    SECTION("Actor with Nop") {
        class IncrementsMapValuesWithNop : public IncrementsMapValues {
        public:
            IncrementsMapValuesWithNop(ActorContext& actorContext,
                                       std::unordered_map<int, int>& counters)
                : IncrementsMapValues(actorContext, counters) {}

            void run() override {
                for (auto&& cfg : _loop) {
                    auto num = cfg.phaseNumber();
                    // This is just for testing purposes. Actors *should* not place any commands
                    // between the top level for-loop and the inner loop.
                    check(num, this->_counters);
                    if (num == 0 || num == 2 || num == 3) {
                        REQUIRE(cfg.isNop());
                    }
                    for (auto&& _ : cfg) {
                        REQUIRE((num == 1 || num == 4));
                        ++this->_counters[cfg->_key];
                    }
                }
            }

            void check(PhaseNumber num, std::unordered_map<int, int>& counter) {
                if (num == 1) {
                    REQUIRE(counter == std::unordered_map<int, int>{});
                }
                if (num == 2 || num == 3 || num == 4) {
                    // std::cout << "Counter at num234 : " << counter << std::endl;
                    REQUIRE(counter == std::unordered_map<int, int>{{72, 10}});
                }
                if (num == 5) {
                    // std::cout << "Counter at num5 : " << counter << std::endl;
                    REQUIRE(counter == std::unordered_map<int, int>{{72, 10}, {94, 3}});
                }
            }
        };

        // This is how a Nop command should be specified.
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Phase: 0
                Nop: true
              - Repeat: 10
                Key: 71
              - Nop: true
              - Nop: true
              - Repeat: 3
                Key: 93
              - Nop: true
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");

        ActorHelper ah(config, 1, {{"Inc", imvProducer}});
        ah.run();

        REQUIRE(imvProducer->counters ==
                std::unordered_map<int, int>{
                    {72, 10},  // keys & vals came from yaml config. Keys have a +1 offset.
                    {94, 3}});
    }

    SECTION("SleepBefore and SleepAfter") {
        using namespace std::literals::chrono_literals;
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Repeat: 3
                SleepBefore: 50 milliseconds
                SleepAfter: 100 milliseconds
                Key: 71
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");
        ActorHelper ah(config, 1, {{"Inc", imvProducer}});

        auto start = std::chrono::high_resolution_clock::now();
        ah.run();

        auto duration = std::chrono::high_resolution_clock::now() - start;

        REQUIRE(duration > 450ms);
        REQUIRE(duration < 550ms);
    }

    SECTION("SleepBefore < 0") {
        using namespace std::literals::chrono_literals;
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Repeat: 3
                SleepBefore: -10 milliseconds
                SleepAfter: 100 milliseconds
                Key: 71
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");

        REQUIRE_THROWS_WITH(
            ([&]() {
                ActorHelper ah(config, 1, {{"Inc", imvProducer}});
                ah.run();
            }()),
            Catch::Matches("Value for genny::IntegerSpec can't be negative: -10 from config: -10"));
    }

    SECTION("SleepAfter and GlobalRate") {
        using namespace std::literals::chrono_literals;
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Repeat: 3
                SleepBefore: 10 milliseconds
                SleepAfter: 100 milliseconds
                GlobalRate: 20 per 30 milliseconds
                Key: 71
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");

        REQUIRE_THROWS_WITH(([&]() {
                                ActorHelper ah(config, 1, {{"Inc", imvProducer}});
                                ah.run();
                            }()),
                            Catch::Matches(R"(GlobalRate must \*not\* be specified alongside .*)"));
    }

    SECTION("SleepBefore = 0") {
        using namespace std::literals::chrono_literals;
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Type: Inc
              Name: Inc
              Phases:
              - Repeat: 3
                SleepBefore: 0 milliseconds
                SleepAfter: 100 milliseconds
                Key: 71
        )");

        auto imvProducer = std::make_shared<CounterProducer<IncrementsMapValues>>("Inc");
        ActorHelper ah(config, 1, {{"Inc", imvProducer}});

        auto start = std::chrono::high_resolution_clock::now();
        ah.run();

        auto duration = std::chrono::high_resolution_clock::now() - start;

        REQUIRE(duration > 0ms);
        REQUIRE(duration < 350ms);
    }
}
