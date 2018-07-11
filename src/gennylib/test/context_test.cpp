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
using Catch::Matchers::StartsWith;

template<class Out, class... Args>
void errors(const string &yaml, string message, Args...args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator;
    string modified = "SchemaVersion: 2018-07-01\nActors: []\n" + yaml;
    auto read = YAML::Load(modified);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, {}};
        return context.get<Out>(std::forward<Args>(args)...);
    };
    CHECK_THROWS_WITH(test(), StartsWith(message));
}
template<class Out, class... Args>
void gives(const string &yaml, Out expect, Args...args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator;
    string modified = "SchemaVersion: 2018-07-01\nActors: []\n" + yaml;
    auto read = YAML::Load(modified);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, {}};
        return context.get<Out>(std::forward<Args>(args)...);
    };
    REQUIRE(test() == expect);
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
        auto actors = w.get("Actors");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27\nActors: []");

        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, {}); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid schema version"));
    }

    SECTION("Invalid config accesses") {
        // key not found
        errors<string>("Foo: bar", "Invalid key [FoO]", "FoO");
        // yaml library does type-conversion; we just forward through...
        gives<string> ("Foo: 123", "123", "Foo");
        gives<int>    ("Foo: 123", 123, "Foo");
        // ...and propagate errors.
        errors<int>   ("Foo: Bar", "Bad conversion of [Bar] to [i] at path [Foo/]:", "Foo");
        // okay
        gives<int>    ("Foo: [1,\"bar\"]", 1, "Foo", 0);
        // give meaningful error message:
        errors<string>("Foo: [1,\"bar\"]", "Invalid key [0] at path [Foo/0/]. Last accessed [[1, bar]].", "Foo", "0");

        errors<string>("Foo: 7", "Wanted [Foo/Bar] but [Foo/] is scalar: [7]", "Foo", "Bar");
        errors<string>("Foo: 7", "Wanted [Foo/Bar] but [Foo/] is scalar: [7]", "Foo", "Bar", "Baz", "Bat");

        auto other = R"(Other: [{ Foo: [{Key: 1, Another: true, Nested: [false, true]}] }])";

        gives<int>(other, 1,     "Other", 0, "Foo", 0, "Key");
        gives<bool>(other, true, "Other", 0, "Foo", 0, "Another");
        gives<bool>(other, false,"Other", 0, "Foo", 0, "Nested", 0);
        gives<bool>(other, true, "Other", 0, "Foo", 0, "Nested", 1);

        gives<int>("Some Ints: [1,2,[3,4]]", 1, "Some Ints", 0);
        gives<int>("Some Ints: [1,2,[3,4]]", 2, "Some Ints", 1);
        gives<int>("Some Ints: [1,2,[3,4]]", 3, "Some Ints", 2, 0);
        gives<int>("Some Ints: [1,2,[3,4]]", 4, "Some Ints", 2, 1);
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