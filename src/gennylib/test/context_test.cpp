#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <log.hh>


using namespace genny;

using Catch::Matchers::Matches;

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

        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid schema version"));
    }

    SECTION("Access nested structures") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Some Ints: [1,2,[3,4]]
Other: [{ Foo: [{Key: 1, Another: true}] }]
)");
        WorkloadContext w{yaml, metrics, orchestrator, {}};
        REQUIRE(w.get<std::string>("SchemaVersion") == "2018-07-01");
        REQUIRE(w.get<int>("Other", 0, "Foo", 0, "Key") == 1);
        REQUIRE(w.get<bool>("Other", 0, "Foo", 0, "Another"));
        REQUIRE(w.get<int>("Some Ints", 0) == 1);
        REQUIRE(w.get<int>("Some Ints", 1) == 2);
        REQUIRE(w.get<int>("Some Ints", 2, 0) == 3);
        REQUIRE(w.get<int>("Some Ints", 2, 1) == 4);
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[SchemaVersion\] at path.*)"));
    }
    //
    //
    //    SECTION(
    //        "Two ActorProducers can see all Actors blocks and producers continue even if errors "
    //        "reported") {
    //        auto yaml = YAML::Load(R"(
    // SchemaVersion: 2018-07-01
    // Actors:
    //- Name: One
    //  SomeList: [100, 2, 3]
    //- Name: Two
    //  Count: 7
    //  SomeList: [2]
    //        )");
    //
    //        int calls = 0;
    //        std::vector<WorkloadContext::Producer> producers;
    //        producers.emplace_back([&](ActorContext& actoractorContext) {
    //            // purposefully "fail" require
    ////            actoractorContext.require("Name", std::string("One"));
    ////            actoractorContext.require("Count", 5);  // we're type-safe
    ////            actoractorContext.require(actoractorContext["SomeList"], 0, 100);
    //            ++calls;
    //            return WorkloadContext::ActorVector{};
    //        });
    //        producers.emplace_back([&](ActorContext&) {
    //            ++calls;
    //            return WorkloadContext::ActorVector{};
    //        });
    //
    //        auto actors = WorkloadContext{yaml, metrics, orchestrator, producers};
    //
    //        // TODO
    //
    ////        REQUIRE(// reported(actors.errors()) ==
    ////                "" ==
    ////                errString("Key Count not found",
    ////                          "Key Name expect [One] but is [Two]",
    ////                          "Key Count expect [5] but is [7]",
    ////                          "Key 0 expect [100] but is [2]"));
    ////        REQUIRE(calls == 4);
    //    }
}