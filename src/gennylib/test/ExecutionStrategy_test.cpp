#include "test.h"

#include "ActorHelper.hpp"

#include <iostream>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

namespace Catchers = Catch::Matchers;

namespace genny::test {
struct StrategyActor : public Actor {
public:
    struct PhaseState {
        PhaseState(const PhaseContext& context)
            : options{ExecutionStrategy::getOptionsFrom(context, "ExecutionStrategy")},
              throwCount{context.get<int, false>("ThrowCount").value_or(0)} {}

        ExecutionStrategy::RunOptions options;
        signed long long throwCount;
    };

public:
    StrategyActor(ActorContext& context)
        : Actor(context), strategy{context, StrategyActor::id(), "simple"}, _loop{context} {}

    void run() override {
        for (auto && config : _loop) {
            auto throwCount = config->throwCount;
            strategy.run(
                [&]() {
                    if (throwCount > 0) {
                        --throwCount;
                        throw mongocxx::operation_exception();
                    }

                    ++goodRuns;
                },
                config->options);

            auto attempts = strategy.lastResult().numAttempts;
            BOOST_LOG_TRIVIAL(info) << "Phase " << config.phaseNumber() << ": tried " << attempts
                                    << " times";
            allRuns += attempts;

            if (!strategy.lastResult().wasSuccessful) {
                BOOST_LOG_TRIVIAL(info) << "Phase " << config.phaseNumber() << ": failed";
                ++failedRuns;
            }
        }
    };

    static std::string_view defaultName() {
        return "Strategy";
    }

public:
    ExecutionStrategy strategy;
    size_t allRuns = 0;
    size_t failedRuns = 0;
    size_t goodRuns = 0;

private:
    PhaseLoop<PhaseState> _loop;
};

const StrategyActor* extractActor(const std::unique_ptr<Actor>& baseActor) {

    auto actor = dynamic_cast<genny::test::StrategyActor*>(baseActor.get());
    REQUIRE(actor != nullptr);
    return actor;
}
}  // namespace genny::test

TEST_CASE("ExecutionStrategy") {
    mongocxx::instance::current();

    SECTION("Test a clean function") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: Simple
  Type: Strategy
)");

        // Make three phases
        constexpr size_t kPhaseCount = 3;
        auto yamlSimpleActor = config["Actors"][0];
        for (size_t i = 0; i < kPhaseCount; ++i) {
            yamlSimpleActor["Phases"][i]["Phase"] = i;
        }

        auto producer = std::make_shared<genny::DefaultActorProducer<genny::test::StrategyActor>>();
        genny::ActorHelper elf(config, 1, {{"Strategy", producer}});

        auto verifyFun = [&](const genny::WorkloadContext& context) {
            auto actor = genny::test::extractActor(context.actors()[0]);

            REQUIRE(actor->allRuns == kPhaseCount);
            REQUIRE(actor->goodRuns == kPhaseCount);
            REQUIRE(actor->failedRuns == 0);
            REQUIRE(actor->strategy.lastResult().wasSuccessful);
        };

        // Give it a run
        elf.runDefaultAndVerify(verifyFun);

        // Give it another run
        elf.runDefaultAndVerify(verifyFun);

        // Once more, for spirit
        elf.runDefaultAndVerify(verifyFun);
    }

    SECTION("Test default exception catching") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: Simple
  Type: Strategy
)");

        // Make three phases
        constexpr size_t kPhaseCount = 3;
        constexpr size_t kExpectedFailures = 2;
        auto yamlSimpleActor = config["Actors"][0];
        for (size_t i = 0; i < kPhaseCount; ++i) {
            yamlSimpleActor["Phases"][i]["Phase"] = i;
            yamlSimpleActor["Phases"][i]["ThrowCount"] = i;
        }

        auto producer = std::make_shared<genny::DefaultActorProducer<genny::test::StrategyActor>>();
        genny::ActorHelper elf(config, 1, {{"Strategy", producer}});

        auto verifyFun = [&](const genny::WorkloadContext& context) {
            auto actor = genny::test::extractActor(context.actors()[0]);

            // We are willing to throw twice on Phase 2, but we are not retrying, so we our fails
            // match our catches
            REQUIRE(actor->allRuns == kPhaseCount);
            REQUIRE(actor->failedRuns == kExpectedFailures);
            REQUIRE(actor->goodRuns == (kPhaseCount - kExpectedFailures));
            REQUIRE(!actor->strategy.lastResult().wasSuccessful);
        };

        elf.runDefaultAndVerify(verifyFun);
    }

    SECTION("Test retries and failure reset") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: Simple
  Type: Strategy
)");

        // Make three phases
        constexpr size_t kPhaseCount = 3;
        constexpr size_t kExpectedFailures = 2;
        auto yamlSimpleActor = config["Actors"][0];
        for (size_t i = 0; i < kPhaseCount; ++i) {
            yamlSimpleActor["Phases"][i]["Phase"] = i;
        }

        size_t run = 0;
        size_t good = 0;
        size_t failed = 0;

        // Do not throw but be very willing to catch
        yamlSimpleActor["Phases"][0]["ThrowCount"] = 0;
        yamlSimpleActor["Phases"][0]["ExecutionStrategy"]["Retries"] = 10;
        ++run;
        ++good;

        // Throw one more time than we can catch
        constexpr size_t kOverthrows = 4;
        yamlSimpleActor["Phases"][1]["ThrowCount"] = kOverthrows;
        yamlSimpleActor["Phases"][1]["ExecutionStrategy"]["Retries"] = kOverthrows - 1;
        run += kOverthrows;
        ++failed;

        // Throw exactly as many times as we can catch (3 runs, 2 caught, 0 failed)
        constexpr size_t kMatchedThrows = 2;
        yamlSimpleActor["Phases"][2]["ThrowCount"] = kMatchedThrows;
        yamlSimpleActor["Phases"][2]["ExecutionStrategy"]["Retries"] = kMatchedThrows;
        run += kMatchedThrows + 1;
        ++good;

        auto producer = std::make_shared<genny::DefaultActorProducer<genny::test::StrategyActor>>();
        genny::ActorHelper elf(config, 1, {{"Strategy", producer}});

        auto verifyFun = [&](const genny::WorkloadContext& context) {
            auto actor = genny::test::extractActor(context.actors()[0]);

            REQUIRE(actor->allRuns == run);
            REQUIRE(actor->failedRuns == failed);
            REQUIRE(actor->goodRuns == good);
            REQUIRE(actor->strategy.lastResult().wasSuccessful);
        };

        elf.runDefaultAndVerify(verifyFun);
    }
}
