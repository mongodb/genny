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


#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
#include <thread>

#include <yaml-cpp/yaml.h>

#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <metrics/metrics.hpp>
#include <testlib/ActorHelper.hpp>

#include <testlib/helpers.hpp>

using namespace genny;
using namespace std;

using Catch::Matchers::Matches;
using Catch::Matchers::StartsWith;

// The driver checks the passed-in mongo uri for accuracy but doesn't actually
// initiate a connection until a connection is retrieved from
// the connection-pool
static constexpr std::string_view mongoUri = "mongodb://localhost:27017";

template <class Out, class... Args>
void errors(const string& yaml, string message, Args... args) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    string modified = "SchemaVersion: 2018-07-01\nActors: []\n" + yaml;
    auto read = YAML::Load(modified);
    auto test = [&]() {
        auto context = WorkloadContext{read, metrics, orchestrator, mongoUri.data(), Cast{}};
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
        auto context = WorkloadContext{read, metrics, orchestrator, mongoUri.data(), Cast{}};
        return context.get<Out, Required>(std::forward<Args>(args)...);
    };
    REQUIRE(test() == expect);
}

struct NoOpProducer : public ActorProducer {
    NoOpProducer() : ActorProducer("NoOp") {}

    ActorVector produce(ActorContext& context) override {
        return {};
    }
};

struct OpProducer : public ActorProducer {
    OpProducer(std::function<void(ActorContext&)> op) : ActorProducer("Op"), _op(op) {}

    ActorVector produce(ActorContext& context) override {
        _op(context);
        return {};
    }

    std::function<void(ActorContext&)> _op;
};

TEST_CASE("loads configuration okay") {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};

    auto cast = Cast{
        {"NoOp", std::make_shared<NoOpProducer>()},
    };

    SECTION("Valid YAML") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: HelloWorld
  Type: NoOp
  Count: 7
        )");

        WorkloadContext w{yaml, metrics, orchestrator, mongoUri.data(), cast};
        auto actors = w.get("Actors");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27\nActors: []");

        auto test = [&]() {
            WorkloadContext w(yaml, metrics, orchestrator, mongoUri.data(), cast);
        };
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
        auto test = [&]() {
            WorkloadContext w(yaml, metrics, orchestrator, mongoUri.data(), cast);
        };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[SchemaVersion\] at path(.*\n*)*)"));
    }
    SECTION("No Actors") {
        auto yaml = YAML::Load("SchemaVersion: 2018-07-01");
        auto test = [&]() {
            WorkloadContext w(yaml, metrics, orchestrator, mongoUri.data(), cast);
        };
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key \[Actors\] at path(.*\n*)*)"));
    }
    SECTION("Invalid MongoUri") {
        auto yaml = YAML::Load("SchemaVersion: 2018-07-01\nActors: []");
        auto test = [&]() { WorkloadContext w(yaml, metrics, orchestrator, "::notValid::", cast); };
        REQUIRE_THROWS_WITH(test(), Matches(R"(an invalid MongoDB URI was provided)"));
    }

    SECTION("Can call two actor producers") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: SomeList
  SomeList: [100, 2, 3]
- Name: Two
  Type: Count
  Count: 7
  SomeList: [2]
        )");

        struct SomeListProducer : public ActorProducer {
            using ActorProducer::ActorProducer;

            ActorVector produce(ActorContext& context) override {
                REQUIRE(context.workload().get<int>("Actors", 0, "SomeList", 0) == 100);
                REQUIRE(context.get<int>("SomeList", 0) == 100);
                ++calls;
                return ActorVector{};
            }

            int calls = 0;
        };

        struct CountProducer : public ActorProducer {
            using ActorProducer::ActorProducer;

            ActorVector produce(ActorContext& context) override {
                REQUIRE(context.workload().get<int>("Actors", 1, "Count") == 7);
                REQUIRE(context.get<int>("Count") == 7);
                ++calls;
                return ActorVector{};
            }

            int calls = 0;
        };

        auto someListProducer = std::make_shared<SomeListProducer>("SomeList");
        auto countProducer = std::make_shared<CountProducer>("Count");

        auto twoActorCast = Cast{
            {"SomeList", someListProducer},
            {"Count", countProducer},
        };

        auto context = WorkloadContext{yaml, metrics, orchestrator, mongoUri.data(), twoActorCast};

        REQUIRE(someListProducer->calls == 1);
        REQUIRE(countProducer->calls == 1);
        REQUIRE(std::distance(context.actors().begin(), context.actors().end()) == 0);
    }

    SECTION("Will throw if Producer is defined again") {
        auto testFun = []() {
            auto noOpProducer = std::make_shared<NoOpProducer>();
            auto cast = Cast{
                {"Foo", noOpProducer},
                {"Bar", noOpProducer},
                {"Foo", noOpProducer},
            };
        };
        REQUIRE_THROWS_WITH(testFun(), StartsWith(R"(Failed to add 'NoOp' as 'Foo')"));
    }
}

void onContext(YAML::Node yaml, std::function<void(ActorContext&)> op) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};

    auto cast = Cast{
        {"Op", std::make_shared<OpProducer>(op)},
        {"NoOp", std::make_shared<NoOpProducer>()},
    };

    WorkloadContext{yaml, metrics, orchestrator, mongoUri.data(), cast};
}

TEST_CASE("PhaseContexts constructed as expected") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Name: HelloWorld
      Type: Op
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

TEST_CASE("Duplicate Phase Numbers") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Type: NoOp
      Phases:
      - Phase: 0
      - Phase: 0
    )");

    metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};

    auto cast = Cast{
        {"NoOp", std::make_shared<NoOpProducer>()},
    };

    REQUIRE_THROWS_WITH((WorkloadContext{yaml, metrics, orchestrator, mongoUri.data(), cast}),
                        Catch::Matches("Duplicate phase 0"));
}

TEST_CASE("No PhaseContexts") {
    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    MongoUri: mongodb://localhost:27017
    Actors:
    - Name: HelloWorld
      Type: NoOp
    )");

    SECTION("Empty PhaseContexts") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().size() == 0);
        };
        onContext(yaml, op);
    }
}

TEST_CASE("Actors Share WorkloadContext State") {

    struct PhaseConfig {
        PhaseConfig(PhaseContext& ctx) {}
    };

    class DummyInsert : public Actor {
    public:
        struct InsertCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

        DummyInsert(ActorContext& actorContext)
            : Actor(actorContext),
              _loop{actorContext},
              _iCounter{WorkloadContext::getActorSharedState<DummyInsert, InsertCounter>()} {}

        void run() override {
            for (auto&& cfg : _loop) {
                for (auto&& _ : cfg) {
                    BOOST_LOG_TRIVIAL(info) << "Inserting document at: " << _iCounter;
                    ++_iCounter;
                }
            }
        }

        static std::string_view defaultName() {
            return "DummyInsert";
        }

    private:
        PhaseLoop<PhaseConfig> _loop;
        InsertCounter& _iCounter;
    };

    class DummyFind : public Actor {
    public:
        DummyFind(ActorContext& actorContext)
            : Actor(actorContext),
              _loop{actorContext},
              _iCounter{
                  WorkloadContext::getActorSharedState<DummyInsert, DummyInsert::InsertCounter>()} {
        }

        void run() override {
            for (auto&& cfg : _loop) {
                for (auto&& _ : cfg) {
                    BOOST_LOG_TRIVIAL(info) << "Finding document lower than: " << _iCounter;
                }
            }
        }

        static std::string_view defaultName() {
            return "DummyFind";
        }

    private:
        PhaseLoop<PhaseConfig> _loop;
        DummyInsert::InsertCounter& _iCounter;
    };

    auto insertProducer = std::make_shared<DefaultActorProducer<DummyInsert>>("DummyInsert");
    auto findProducer = std::make_shared<DefaultActorProducer<DummyFind>>("DummyFind");

    YAML::Node config = YAML::Load(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: DummyInsert
          Type: DummyInsert
          Threads: 10
          Phases:
          - Repeat: 10
        - Name: DummyFind
          Type: DummyFind
          Threads: 10
          Phases:
          - Repeat: 10
    )");

    ActorHelper ah(config, 20, {{"DummyInsert", insertProducer}, {"DummyFind", findProducer}});
    ah.run();

    REQUIRE(WorkloadContext::getActorSharedState<DummyInsert, DummyInsert::InsertCounter>() ==
            10 * 10);
}

struct TakesInt {
    int value;
    TakesInt(int x) : value{x} {
        if (x > 7) {
            throw std::logic_error("Expected");
        }
    }
};

struct AnotherInt : public TakesInt {
    // need no-arg ctor to be able to do YAML::Node::as<>()
    AnotherInt() : AnotherInt(0) {}
    AnotherInt(int x) : TakesInt(x) {}
};

namespace YAML {

template <>
struct convert<AnotherInt> {
    static Node encode(const AnotherInt& rhs) {
        return {};
    }
    static bool decode(const Node& node, AnotherInt& rhs) {
        rhs = AnotherInt{node.as<int>()};
        return true;
    }
};

}  // namespace YAML

TEST_CASE("getPlural") {
    auto createYaml = [](std::string actorYaml) {
        auto doc = YAML::Load(R"(
SchemaVersion: 2018-07-01
Numbers: [1,2,3]
Actors: [{}]
)");
        auto actor = YAML::Load(actorYaml);
        actor["Type"] = "Op";
        doc["Actors"][0] = actor;
        return doc;
    };

    // can use built-in decode types
    onContext(createYaml("Foo: 5"), [](ActorContext& c) {
        c.getPlural<TakesInt>("Foo", "Foos", [](YAML::Node n) { return TakesInt{n.as<int>()}; });
    });

    onContext(createYaml("Foo: 5"), [](ActorContext& c) {
        REQUIRE(c.getPlural<AnotherInt>("Foo", "Foos")[0].value == 5);
    });

    onContext(createYaml("{}"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH([&]() { c.getPlural<int>("Foo", "Foos"); }(),
                            Matches("Either 'Foo' or 'Foos' required."));
    });
    onContext(createYaml("Foo: 81"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH(
            [&]() {
                c.getPlural<TakesInt>(
                    "Foo", "Foos", [](YAML::Node n) { return TakesInt{n.as<int>()}; });
            }(),
            Matches("Expected"));
    });

    onContext(createYaml("Foos: [733]"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Foo", "Foos") == std::vector<int>{733});
    });

    onContext(createYaml("Foos: 73"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH([&]() { c.getPlural<int>("Foo", "Foos"); }(),
                            Matches("'Foos' must be a sequence type."));
    });

    onContext(createYaml("Foo: 71"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Foo", "Foos") == std::vector<int>{71});
    });

    onContext(createYaml("{ Foo: 9, Foos: 1 }"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH([&]() { c.getPlural<int>("Foo", "Foos"); }(),
                            Matches("Can't have both 'Foo' and 'Foos'."));
    });

    onContext(createYaml("Number: 7"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH([&]() { c.getPlural<int>("Number", "Numbers"); }(),
                            Matches("Can't have both 'Number' and 'Numbers'."));
    });

    onContext(createYaml("Numbers: [3, 4, 5]"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Number", "Numbers") == std::vector<int>{3, 4, 5});
    });
}

TEST_CASE("Configuration cascades to nested context types") {
    auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Database: test
Actors:
- Name: Actor1
  Type: Op
  Collection: mycoll
  Phases:
  - Operation: Nop

  - Operation: Insert
    Database: test3
    Collection: mycoll2

- Name: Actor2
  Type: Op
  Database: test2
    )");

    SECTION("ActorContext inherits from WorkloadContext") {
        onContext(yaml, [](ActorContext& actorContext) {
            const auto& workloadContext = actorContext.workload();
            REQUIRE(workloadContext.get_noinherit<std::string>("Database") == "test");
            REQUIRE(workloadContext.get<std::string>("Database") == "test");

            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            REQUIRE((actorName == "Actor1" || actorName == "Actor2"));

            if (actorName == "Actor1") {
                REQUIRE(actorContext.get_noinherit<std::string, false>("Database") == std::nullopt);

                REQUIRE_THROWS_WITH(
                    ([&]() { actorContext.get_noinherit<std::string, true>("Database"); })(),
                    Matches(R"(Invalid key \[Database\] at path(.*\n*)*)"));

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
            REQUIRE((actorName == "Actor1" || actorName == "Actor2"));

            if (actorName == "Actor1") {
                REQUIRE(actorContext.get_noinherit<std::string>("Collection") == "mycoll");
                REQUIRE(actorContext.get<std::string>("Collection") == "mycoll");

                for (auto&& [phase, config] : actorContext.phases()) {
                    REQUIRE((phase == 0 || phase == 1));

                    if (phase == 0) {
                        REQUIRE(config->get_noinherit<std::string, false>("Collection") ==
                                std::nullopt);

                        const auto* rawConfig = config.get();
                        REQUIRE_THROWS_WITH(
                            ([&]() {
                                rawConfig->get_noinherit<std::string, true>("Collection");
                            })(),
                            Matches(R"(Invalid key \[Collection\] at path(.*\n*)*)"));

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
            REQUIRE((actorName == "Actor1" || actorName == "Actor2"));

            if (actorName == "Actor1") {
                for (auto&& [phase, config] : actorContext.phases()) {
                    REQUIRE((phase == 0 || phase == 1));

                    if (phase == 0) {
                        REQUIRE(config->get_noinherit<std::string, false>("Database") ==
                                std::nullopt);

                        const auto* rawConfig = config.get();
                        REQUIRE_THROWS_WITH(
                            ([&]() { rawConfig->get_noinherit<std::string, true>("Database"); })(),
                            Matches(R"(Invalid key \[Database\] at path(.*\n*)*)"));

                        REQUIRE(config->get<std::string>("Database") == "test");
                    } else if (phase == 1) {
                        REQUIRE(config->get_noinherit<std::string>("Database") == "test3");
                        REQUIRE(config->get<std::string>("Database") == "test3");
                    }
                }
            }
        });
    }

    SECTION("Nested contexts can have different types for the same named key") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
MiscField: {a: b}
Actors:
- Name: Actor
  Type: Op
  MiscField: c
  Phases:
  - MiscField: [1, 2, 3]
    )");

        onContext(yaml, [](ActorContext& actorContext) {
            const auto& workloadContext = actorContext.workload();

            REQUIRE(workloadContext.get_noinherit<std::map<std::string, std::string>>(
                        "MiscField") == std::map<std::string, std::string>{{"a", "b"}});
            REQUIRE(workloadContext.get<std::map<std::string, std::string>>("MiscField") ==
                    std::map<std::string, std::string>{{"a", "b"}});

            const auto actorName = actorContext.get_noinherit<std::string>("Name");
            REQUIRE(actorName == "Actor");

            REQUIRE(actorContext.get_noinherit<std::string>("MiscField") == "c");
            REQUIRE(actorContext.get<std::string>("MiscField") == "c");

            REQUIRE_THROWS_WITH(
                ([&]() {
                    actorContext.get_noinherit<std::map<std::string, std::string>, true>(
                        "MiscField");
                })(),
                Matches(R"(Bad conversion of \[c\] to(.*\n*)*)"));
            REQUIRE_THROWS_WITH(([&]() {
                                    actorContext.get<std::map<std::string, std::string>, true>(
                                        "MiscField");
                                })(),
                                Matches(R"(Bad conversion of \[c\] to(.*\n*)*)"));

            for (auto&& [phase, config] : actorContext.phases()) {
                REQUIRE(phase == 0);

                REQUIRE(config->get_noinherit<std::vector<int>>("MiscField") ==
                        std::vector<int>{1, 2, 3});
                REQUIRE(config->get<std::vector<int>>("MiscField") == std::vector<int>{1, 2, 3});

                const auto* rawConfig = config.get();
                REQUIRE_THROWS_WITH(
                    ([&]() {
                        rawConfig->get_noinherit<std::map<std::string, std::string>, true>(
                            "MiscField");
                    })(),
                    Matches(R"(Bad conversion of \[\[1, 2, 3\]\] to(.*\n*)*)"));
                REQUIRE_THROWS_WITH(([&]() {
                                        rawConfig->get<std::map<std::string, std::string>, true>(
                                            "MiscField");
                                    })(),
                                    Matches(R"(Bad conversion of \[\[1, 2, 3\]\] to(.*\n*)*)"));
                REQUIRE_THROWS_WITH(
                    ([&]() { rawConfig->get_noinherit<std::string, true>("MiscField"); })(),
                    Matches(R"(Bad conversion of \[\[1, 2, 3\]\] to(.*\n*)*)"));
                REQUIRE_THROWS_WITH(([&]() { rawConfig->get<std::string, true>("MiscField"); })(),
                                    Matches(R"(Bad conversion of \[\[1, 2, 3\]\] to(.*\n*)*)"));
            }
        });
    }
}

TEST_CASE("If no producer exists for an actor, then we should throw an error") {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};

    auto cast = Cast{
        {"Foo", std::make_shared<NoOpProducer>()},
    };

    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    Database: test
    Actors:
    - Name: Actor1
      Type: Bar
    )");

    SECTION("Incorrect type value inputted") {
        auto test = [&]() {
            WorkloadContext w(yaml, metrics, orchestrator, mongoUri.data(), cast);
        };
        REQUIRE_THROWS_WITH(
            test(), Matches(R"(Unable to construct actors: No producer for 'Bar'(.*\n*)*)"));
    }
}
