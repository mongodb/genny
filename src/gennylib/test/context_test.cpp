#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <log.hh>


using namespace genny;
using namespace std;

using Catch::Matchers::Matches;

template<class... Args>
void errors(string yaml, string expect, Args...args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator;
    auto read = YAML::Load(yaml);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, {}};
        context.get(std::forward<Args>(args)...);
    };

    CHECK_THROWS_WITH(test(), expect);
}

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
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27\nActors: []");

        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid schema version"));
    }

    SECTION("Invalid config") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27\nActors: []");

        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid schema version"));
    }

    SECTION("Invalid config accesses") {

    }

    SECTION("Access nested structures") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors: []
Some Ints: [1,2,[3,4]]
Other: [{ Foo: [{Key: 1, Another: true, Nested: [false, true]}] }]
)");
        WorkloadContext w{yaml, metrics, orchestrator, {}};
        CHECK(w.get<std::string>("SchemaVersion") == "2018-07-01");
        CHECK(w.get<int>("Other", 0, "Foo", 0, "Key") == 1);
        CHECK(w.get<bool>("Other", 0, "Foo", 0, "Another"));
        CHECK(w.get<bool>("Other", 0, "Foo", 0, "Nested", 0) == false);
        CHECK(w.get<bool>("Other", 0, "Foo", 0, "Nested", 1) == true);
        CHECK(w.get<int>("Some Ints", 0) == 1);
        CHECK(w.get<int>("Some Ints", 1) == 2);
        CHECK(w.get<int>("Some Ints", 2, 0) == 3);
        CHECK(w.get<int>("Some Ints", 2, 1) == 4);
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("Actors: []");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[SchemaVersion\] at path.*)"));
    }
    SECTION("No Actors") {
        auto yaml = YAML::Load("SchemaVersion: 2018-07-01");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[Actors\] at path.*)"));
    }

    SECTION("Can call two actor producers") {
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
        std::vector<ActorProducer> producers;
        producers.emplace_back([&](ActorContext& context) {
            REQUIRE(context.workload().get<int>("Actors", 0, "SomeList", 0) == 100);
            ++calls;
            return ActorVector{};
        });
        producers.emplace_back([&](ActorContext& context) {
            REQUIRE(context.workload().get<int>("Actors", 1, "Count") == 7);
            ++calls;
            return ActorVector{};
        });

        auto context = WorkloadContext{yaml, metrics, orchestrator, producers};
        REQUIRE(std::distance(context.actors().begin(), context.actors().end()) == 0);
    }
}