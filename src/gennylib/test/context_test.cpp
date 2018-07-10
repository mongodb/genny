#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>


using namespace genny;

TEST_CASE("loads configuration okay") {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator;
    SECTION("Valid YAML") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: HelloWorld
  Count: 7
        )");
        WorkloadContext w{yaml, metrics, orchestrator, {}};
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27");

        auto test = [&]() { WorkloadContext w{yaml, metrics, orchestrator, {}}; };
        REQUIRE_THROWS_WITH(test, "Invalid schema version");
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("");
        auto result = WorkloadContext{yaml, metrics, orchestrator, {}};
//        REQUIRE((bool)result.errors());
//        REQUIRE(reported(result.errors()) == errString("Key SchemaVersion not found"));
    }


    SECTION(
        "Two ActorProducers can see all Actors blocks and producers continue even if errors "
        "reported") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  SomeList: [100, 2, 3]
- Name: Two
  Count: 7
  SomeList: [2]
        )");

        int calls = 0;
        std::vector<WorkloadContext::Producer> producers;
        producers.emplace_back([&](ActorContext& actoractorContext) {
            // purposefully "fail" require
//            actoractorContext.require("Name", std::string("One"));
//            actoractorContext.require("Count", 5);  // we're type-safe
//            actoractorContext.require(actoractorContext["SomeList"], 0, 100);
            ++calls;
            return WorkloadContext::ActorVector{};
        });
        producers.emplace_back([&](ActorContext&) {
            ++calls;
            return WorkloadContext::ActorVector{};
        });

        auto actors = WorkloadContext{yaml, metrics, orchestrator, producers};

        // TODO

//        REQUIRE(// reported(actors.errors()) ==
//                "" ==
//                errString("Key Count not found",
//                          "Key Name expect [One] but is [Two]",
//                          "Key Count expect [5] but is [7]",
//                          "Key 0 expect [100] but is [2]"));
//        REQUIRE(calls == 4);
    }
}