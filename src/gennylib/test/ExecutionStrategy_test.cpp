#include "test.h"

#include "ActorHelper.hpp"

#include <iostream>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <yaml-cpp/yaml.h>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace Catchers = Catch::Matchers;

namespace genny::test {
struct StrategyActor : public Actor {
public:
    struct PhaseState{
        PhaseState(const PhaseContext & context){
            auto maybeOptions = context.get<config::ExecutionStrategy, false>("ExecutionStrategy");
            if (maybeOptions)
                options = *maybeOptions;
        }

        ExecutionStrategy::RunOptions options;
    };
public:
    StrategyActor(ActorContext& context)
        : Actor(context), strategy{context, StrategyActor::id(), "simple"}, _loop{context} {}

    void run() override {
        for (auto && [ phase, config ] : _loop) {
            for (const auto&& _ : config) {
                strategy.run([&]() { ++calls; }, config->options);
            }
        }
    };

    static std::string_view defaultName() {
        return "Strategy";
    }

public:
    ExecutionStrategy strategy;
    size_t calls = 0;

private:
    PhaseLoop<PhaseState> _loop;
};
}  // namespace genny::test

TEST_CASE("ExecutionStrategy") {
    mongocxx::instance::current();

    SECTION("Run a simple function with and without custom options"){
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: Simple
  Type: Strategy
)");

        // Make three phases
        constexpr size_t kPhaseCount = 3;
        for (size_t i = 0; i < kPhaseCount; ++i) {
            config["Actors"][0]["Phases"][i]["Phase"] = i;
        }

        auto producer = std::make_shared<genny::DefaultActorProducer<genny::test::StrategyActor>>();
        genny::ActorHelper elf(config, 1, {{"Strategy", producer}});
        elf.runDefaultAndVerify([&](const genny::WorkloadContext& context) {
            auto & baseActor = context.actors()[0];
            auto simpleActor = dynamic_cast<const genny::test::StrategyActor*>(baseActor.get());
            REQUIRE(simpleActor != nullptr);

            REQUIRE(simpleActor->calls == kPhaseCount);
        });
    }
}
