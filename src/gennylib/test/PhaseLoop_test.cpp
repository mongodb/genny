#include "test.h"

#include <chrono>
#include <iostream>
#include <optional>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <log.hh>

using namespace genny;
using namespace genny::V1;
using namespace std;

namespace {

//
// Cute convenience operators -
//  100_i   gives optional<int>     holding 100
//  100_ms  gives optional<millis>  holding 100
//
// These are copy/pasta in PhaseLoop_test and orchestrator_test. Refactor.
optional<int> operator"" _i(unsigned long long int v) {
    return make_optional(v);
}

optional<chrono::milliseconds> operator"" _ms(unsigned long long int v) {
    return make_optional(chrono::milliseconds{v});
}

}  // namespace

TEST_CASE("Correctness for N iterations") {
    Orchestrator o;

    SECTION("Loops 0 Times") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(nullopt, 0_i, false), 1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 0);
    }
    SECTION("Loops 1 Time") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(nullopt, 1_i, false), 1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 1);
    }
    SECTION("Loops 113 Times") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(nullopt, 113_i, false), 1};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 113);
    }

    SECTION("Configured for -1 Times barfs") {
        REQUIRE_THROWS_WITH((V1::ActorPhase<int>{o,
                                                 std::make_unique<V1::IterationCompletionCheck>(
                                                     nullopt, make_optional(-1), false),
                                                 1}),
                            Catch::Contains("Need non-negative number of iterations. Gave -1"));
    }
}

TEST_CASE("Correctness for N milliseconds") {
    Orchestrator o;
    SECTION("Loops 0 milliseconds so zero times") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(0_ms, nullopt, false), 0};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 0);
    }
    SECTION("Looping for 10 milliseconds takes between 10 and 11 milliseconds") {
        // we nop in the loop so ideally it should take exactly 10ms, but don't want spurious
        // failures
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(10_ms, nullopt, false), 0};

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
    Orchestrator o;
    SECTION("Loops 0 milliseconds but 100 times") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(0_ms, 100_i, false), 0};
        int i = 0;
        for (auto _ : loop)
            ++i;
        REQUIRE(i == 100);
    }
    SECTION("Loops 5 milliseconds, 100 times: 10 millis dominates") {
        V1::ActorPhase<int> loop{
            o, std::make_unique<V1::IterationCompletionCheck>(5_ms, 100_i, false), 0};

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
    // e.g. OperationLoop loop{1000000000000_i, 1_ms} but that would fail spuriously
    // on super-fast hardware. So resist the temptation and trust the logical
    // combinations of the other tests 🙈

    SECTION("Configured for -1 milliseconds barfs") {
        REQUIRE_THROWS_WITH(
            (V1::ActorPhase<int>{o,
                                 std::make_unique<V1::IterationCompletionCheck>(
                                     make_optional(chrono::milliseconds{-1}), nullopt, false),
                                 0}),
            Catch::Contains("Need non-negative duration. Gave -1 milliseconds"));
    }
}

TEST_CASE("Can do without either iterations or duration") {
    Orchestrator o;
    V1::ActorPhase<int> actorPhase{
        o, std::make_unique<V1::IterationCompletionCheck>(nullopt, nullopt, false), 0};
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
    Orchestrator o;
    V1::ActorPhase<int> loop{
        o, std::make_unique<V1::IterationCompletionCheck>(nullopt, 1_i, false), 0};

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
            : _loop{actorContext, 1}, _counters{counters} {}
        //                        ↑ is forwarded to the IncrementsMapValues ctor as the keyOffset
        //                        param.

        void run() override {
            for (auto && [ num, cfg ] : _loop) {
                for (auto&& _ : cfg) {
                    ++this->_counters[cfg->_key];
                }
            }
        }

        // An ActorProducer can do whatever it wants.
        // Here it passes in a stack-variable ref to the Actor(s) it creates.
        static auto producer(std::unordered_map<int, int>& counters) {
            return [&](ActorContext& actorContext) {
                ActorVector out;
                out.push_back(std::make_unique<IncrementsMapValues>(actorContext, counters));
                return out;
            };
        }
    };

    SECTION("Simple Actor") {
        std::unordered_map<int, int> counters;

        // ////////
        // setup and run (bypass the driver)
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Phases:
              - Repeat: 100
                Key: 71
              - Repeat: 3
                Key: 93
        )");

        Orchestrator orchestrator;
        orchestrator.addRequiredTokens(1);

        metrics::Registry registry;
        WorkloadContext wl{config,
                           registry,
                           orchestrator,
                           "mongodb://localhost:27017",
                           {IncrementsMapValues::producer(counters)}};
        wl.actors()[0]->run();
        // end
        // ////////

        REQUIRE(counters ==
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
                for (auto && [ num, cfg ] : _loop) {
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

            static auto producer(std::unordered_map<int, int>& counters) {
                return [&](ActorContext& actorContext) {
                    ActorVector out;
                    out.push_back(
                        std::make_unique<IncrementsMapValuesWithNop>(actorContext, counters));
                    return out;
                };
            }
        };

        std::unordered_map<int, int> counters;

        // This is how a Nop command should be specified.
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Phases:
              - Operation: Nop
              - Repeat: 10
                Key: 71
              - Operation: Nop
              - Operation: Nop
              - Repeat: 3
                Key: 93
              - Operation: Nop
        )");

        Orchestrator orchestrator;
        orchestrator.addRequiredTokens(1);

        metrics::Registry registry;
        WorkloadContext wl{config,
                           registry,
                           orchestrator,
                           "mongodb://localhost:27017",
                           {IncrementsMapValuesWithNop::producer(counters)}};
        wl.actors()[0]->run();
        // end
        // ////////

        REQUIRE(counters ==
                std::unordered_map<int, int>{
                    {72, 10},  // keys & vals came from yaml config. Keys have a +1 offset.
                    {94, 3}});
    }
}