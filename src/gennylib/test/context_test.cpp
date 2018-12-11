#include "test.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>

#include <yaml-cpp/yaml.h>

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <log.hh>


using namespace genny;
using namespace std;

using Catch::Matchers::Matches;
using Catch::Matchers::StartsWith;

// The driver checks the passed-in mongo uri for accuracy but doesn't actually
// initiate a connection until a connection is retrieved from
// the connection-pool
const std::string mongoUri = "mongodb://localhost:27017";

template <class Out, class... Args>
void errors(const string& yaml, string message, Args... args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    string modified = "SchemaVersion: 2018-07-01\nActors: []\n" + yaml;
    auto read = YAML::Load(modified);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, mongoUri, {}};
        return context.get<Out>(std::forward<Args>(args)...);
    };
    CHECK_THROWS_WITH(test(), StartsWith(message));
}
template <class Out,
          bool Required = true,
          class OutV = typename std::conditional<Required, Out, std::optional<Out>>::type,
          class... Args>
void gives(const string& yaml, OutV expect, Args... args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    string modified = "SchemaVersion: 2018-07-01\nActors: []\n" + yaml;
    auto read = YAML::Load(modified);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, mongoUri, {}};
        return context.get<Out, Required>(std::forward<Args>(args)...);
    };
    REQUIRE(test() == expect);
}

TEST_CASE("loads configuration okay") {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    SECTION("Valid YAML") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: HelloWorld
  Count: 7
        )");
        WorkloadContext w{yaml, metrics, orchestrator, mongoUri, {}};
        auto actors = w.get("Actors");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27\nActors: []");

        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, mongoUri, {}); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid schema version"));
    }

    SECTION("Invalid config accesses") {
        // key not found
        errors<string>("Foo: bar", "Invalid key [FoO]", "FoO");
        // yaml library does type-conversion; we just forward through...
        gives<string>("Foo: 123", "123", "Foo");
        gives<int>("Foo: 123", 123, "Foo");
        // ...and propagate errors.
        errors<int>("Foo: Bar", "Bad conversion of [Bar] to [i] at path [Foo/]:", "Foo");
        // okay
        gives<int>("Foo: [1,\"bar\"]", 1, "Foo", 0);
        // give meaningful error message:
        errors<string>("Foo: [1,\"bar\"]",
                       "Invalid key [0] at path [Foo/0/]. Last accessed [[1, bar]].",
                       "Foo",
                       "0");

        errors<string>("Foo: 7", "Wanted [Foo/Bar] but [Foo/] is scalar: [7]", "Foo", "Bar");
        errors<string>(
            "Foo: 7", "Wanted [Foo/Bar] but [Foo/] is scalar: [7]", "Foo", "Bar", "Baz", "Bat");

        auto other = R"(Other: [{ Foo: [{Key: 1, Another: true, Nested: [false, true]}] }])";

        gives<int>(other, 1, "Other", 0, "Foo", 0, "Key");
        gives<bool>(other, true, "Other", 0, "Foo", 0, "Another");
        gives<bool>(other, false, "Other", 0, "Foo", 0, "Nested", 0);
        gives<bool>(other, true, "Other", 0, "Foo", 0, "Nested", 1);

        gives<int>("Some Ints: [1,2,[3,4]]", 1, "Some Ints", 0);
        gives<int>("Some Ints: [1,2,[3,4]]", 2, "Some Ints", 1);
        gives<int>("Some Ints: [1,2,[3,4]]", 3, "Some Ints", 2, 0);
        gives<int>("Some Ints: [1,2,[3,4]]", 4, "Some Ints", 2, 1);

        gives<int, false>("A: 1", std::nullopt, "B");
        gives<int, false>("A: 2", make_optional<int>(2), "A");
        gives<int, false>("A: {B: [1,2,3]}", make_optional<int>(2), "A", "B", 1);

        gives<int, false>("A: {B: [1,2,3]}", std::nullopt, "A", "B", 30);
        gives<int, false>("A: {B: [1,2,3]}", std::nullopt, "B");
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("Actors: []");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, mongoUri, {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[SchemaVersion\] at path(.*\n*)*)"));
    }
    SECTION("No Actors") {
        auto yaml = YAML::Load("SchemaVersion: 2018-07-01");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, mongoUri, {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[Actors\] at path(.*\n*)*)"));
    }
    SECTION("Invalid MongoUri") {
        auto yaml = YAML::Load("SchemaVersion: 2018-07-01\nActors: []");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, "notValid", {}); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(an invalid MongoDB URI was provided)"));
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

        auto context = WorkloadContext{yaml, metrics, orchestrator, mongoUri, producers};
        REQUIRE(std::distance(context.actors().begin(), context.actors().end()) == 0);
    }
}

void onContext(YAML::Node& yaml, std::function<void(ActorContext&)> op) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};

    ActorProducer producer = [&](ActorContext& context) -> ActorVector {
        op(context);
        return {};
    };

    WorkloadContext{yaml, metrics, orchestrator, mongoUri, {producer}};
}

TEST_CASE("PhaseContexts constructed as expected") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Name: HelloWorld
      Foo: Bar
      Foo2: Bar2
      Phases:
      - Operation: One
        Foo: Baz
      - Operation: Two
        Phase: 2 # intentionally out of order for testing
      - Operation: Three
        Phase: 1 # intentionally out of order for testing
        Extra: [1,2]
    )");

    SECTION("Loads Phases") {
        // "test of the test"
        int calls = 0;
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) { ++calls; };
        onContext(yaml, op);
        REQUIRE(calls == 1);
    }

    SECTION("One Phase per block") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            const auto& ph = ctx.phases();
            REQUIRE(ph.size() == 3);
        };
        onContext(yaml, op);
    }
    SECTION("Phase index is defaulted") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().at(0)->get<std::string>("Operation") == "One");
            REQUIRE(ctx.phases().at(1)->get<std::string>("Operation") == "Three");
            REQUIRE(ctx.phases().at(2)->get<std::string>("Operation") == "Two");
        };
        onContext(yaml, op);
    }
    SECTION("Phase values can override parent values") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().at(0)->get<std::string>("Foo") == "Baz");
            REQUIRE(ctx.phases().at(1)->get<std::string>("Foo") == "Bar");
            REQUIRE(ctx.phases().at(2)->get<std::string>("Foo") == "Bar");
        };
        onContext(yaml, op);
    }
    SECTION("Optional values also override") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(*(ctx.phases().at(0)->get<std::string, false>("Foo")) == "Baz");
            REQUIRE(*(ctx.phases().at(1)->get<std::string, false>("Foo")) == "Bar");
            REQUIRE(*(ctx.phases().at(2)->get<std::string, false>("Foo")) == "Bar");
            // call twice just for funsies
            REQUIRE(*(ctx.phases().at(2)->get<std::string, false>("Foo")) == "Bar");
        };
        onContext(yaml, op);
    }
    SECTION("Optional values can be found from parent") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(*(ctx.phases().at(0)->get<std::string, false>("Foo2")) == "Bar2");
            REQUIRE(*(ctx.phases().at(1)->get<std::string, false>("Foo2")) == "Bar2");
            REQUIRE(*(ctx.phases().at(2)->get<std::string, false>("Foo2")) == "Bar2");
        };
        onContext(yaml, op);
    }
    SECTION("Phases can have extra configs") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().at(1)->get<int>("Extra", 0) == 1);
        };
        onContext(yaml, op);
    }
    SECTION("Missing require values throw") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE_THROWS(ctx.phases().at(1)->get<int>("Extra", 100));
        };
        onContext(yaml, op);
    }
}

TEST_CASE("OperationContexts constructed as expected") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Name: Actor1
      Phases:
      - Database: test1
        Operations:
        - MetricsName: Find
          Command:
            find: restaurants
        - MetricsName: Drop
          Command:
            drop: myCollection
      - Database: test2
        Operations:
        - MetricsName: Find
          Command:
            find: schools
      - Operation: Insert
        Database: test3
        Collection: myCollection2
    )");

    SECTION("Loads Phases") {
        // "test of the test"
        int calls = 0;
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) { ++calls; };
        onContext(yaml, op);
        REQUIRE(calls == 1);
    }
    SECTION("Creates the correct number of OperationContexts per phase") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        REQUIRE(config->operations().size() == 2);
                    } else if (phase == 1) {
                        REQUIRE(config->operations().size() == 1);
                    } else if (phase == 2) {
                        REQUIRE(config->operations().size() == 0);
                    }
                }
            }
        });
    }
    SECTION("Operation configs match to the correct phase") {
        auto testYaml = YAML::Load(R"({ rating: { $gte: 9 }, cuisine: italian })");
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        REQUIRE(config->operations().at("Find")->get<std::string>("Command", "find") == "restaurants");
                        REQUIRE(config->operations().at("Drop")->get<std::string>("Command", "drop") == "myCollection");
                        REQUIRE(config->operations().at("Find")->get<std::string>("Database") == "test1");
                    } else if (phase == 1) {
                        REQUIRE(config->operations().at("Find")->get<std::string>("Command", "find") == "schools");
                        REQUIRE(config->operations().at("Find")->get<std::string>("Database") == "test2");
                    }
                }
            }
        });
    }
}

TEST_CASE("Duplicate Phase Numbers") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Phases:
      - Phase: 0
      - Phase: 0
    )");

    metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    ActorProducer producer = [&](ActorContext& context) -> ActorVector { return {}; };

    REQUIRE_THROWS_WITH((WorkloadContext{yaml, metrics, orchestrator, mongoUri, {producer}}),
                        Catch::Matches("Duplicate phase 0"));
}

TEST_CASE("No PhaseContexts") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Name: HelloWorld
    )");

    SECTION("Empty PhaseContexts") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().size() == 0);
        };
        onContext(yaml, op);
    }
}

TEST_CASE("Configuration cascades to nested context types") {
    auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Database: test
Actors:
- Name: Actor1
  Collection: mycoll
  Phases:
  - Operation: Nop

  - Operation: Insert
    Database: test3
    Collection: mycoll2

  - Operations:
    - MetricsName: Find
      Database: test4
      Command:
        find: schools
- Name: Actor2
  Database: test2
    )");

    SECTION("ActorContext inherits from WorkloadContext") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto& workloadContext = actorContext.workload();
            REQUIRE(workloadContext.get_noinherit<std::string>("Database") == "test");
            REQUIRE(workloadContext.get<std::string>("Database") == "test");

            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                REQUIRE(actorContext.get_noinherit<std::string, false>("Database") == std::nullopt);
                REQUIRE(actorContext.get<std::string>("Database") == "test");
            } else if (actorName == "Actor2") {
                REQUIRE(actorContext.get_noinherit<std::string>("Database") == "test2");
                REQUIRE(actorContext.get<std::string>("Database") == "test2");
            }
        });
    }

    SECTION("PhaseContext inherits from ActorContext") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                REQUIRE(actorContext.get_noinherit<std::string>("Collection") == "mycoll");
                REQUIRE(actorContext.get<std::string>("Collection") == "mycoll");

                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        REQUIRE(config->get_noinherit<std::string, false>("Collection") ==
                                std::nullopt);
                        REQUIRE(config->get<std::string>("Collection") == "mycoll");
                    } else if (phase == 1) {
                        REQUIRE(config->get_noinherit<std::string>("Collection") == "mycoll2");
                        REQUIRE(config->get<std::string>("Collection") == "mycoll2");
                    }
                }
            }
        });
    }

    SECTION("PhaseContext inherits from WorkloadContext transitively") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        REQUIRE(config->get_noinherit<std::string, false>("Database") ==
                                std::nullopt);
                        REQUIRE(config->get<std::string>("Database") == "test");
                    } else if (phase == 1) {
                        REQUIRE(config->get_noinherit<std::string>("Database") == "test3");
                        REQUIRE(config->get<std::string>("Database") == "test3");
                    }
                }
            }
        });
    }

    SECTION("OperationContext inherits from ActorContext through PhaseContext") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        for(auto&& [_, opCtx] : config->operations()) {
                            REQUIRE(opCtx->get_noinherit<std::string, false>("Collection") == std::nullopt);
                            REQUIRE(opCtx->get<std::string>("Collection") == "mycoll");
                        }
                    } else if (phase == 1) {
                        for(auto&& [_, opCtx] : config->operations()) {
                            REQUIRE(opCtx->get_noinherit<std::string, false>("Collection") == std::nullopt);
                            REQUIRE(opCtx->get<std::string>("Collection") == "mycoll3");
                        }
                    }
                }
            }
        });
    }

    SECTION("OperationContext inherits from WorkloadContext through PhaseContext") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    if (phase == 0) {
                        for(auto&& [_, opCtx] : config->operations()) {
                            REQUIRE(opCtx->get_noinherit<std::string, false>("Database") == std::nullopt);
                            REQUIRE(opCtx->get<std::string>("Database") == "test");
                        }
                    } else if (phase == 1) {
                        for(auto&& [_, opCtx] : config->operations()) {
                            REQUIRE(opCtx->get_noinherit<std::string, false>("Database") == std::nullopt);
                            REQUIRE(opCtx->get<std::string>("Database") == "test3");
                        }
                    } else if (phase == 2) {
                        for(auto&& [_, opCtx] : config->operations()) {
                            REQUIRE(opCtx->get_noinherit<std::string, false>("Database") == "test4");
                            REQUIRE(opCtx->get<std::string>("Database") == "test4");
                        }
                    }
                }
            }
        });
    }
}
