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
#include <optional>
#include <string_view>
#include <thread>

#include <bsoncxx/json.hpp>

#include <gennylib/Node.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlToBson.hpp>

#include <value_generators/DocumentGenerator.hpp>

using namespace genny;
using namespace std;

using Catch::Matchers::Matches;
using Catch::Matchers::StartsWith;

// The driver checks the passed-in mongo uri for accuracy but doesn't actually
// initiate a connection until a connection is retrieved from
// the connection-pool
static const std::string baseYaml() {
    static std::string out {
        "SchemaVersion: 2018-07-01\nClients: {Default: {URI: 'mongodb://localhost:27017'}}\nActors: []\n"};
    return out;
}

template <typename T, typename Arg, typename... Rest>
auto& applyBracket(const T& t, Arg&& arg, Rest&&... rest) {
    if constexpr (sizeof...(rest) == 0) {
        return t[std::forward<Arg>(arg)];
    } else {
        return applyBracket(t[std::forward<Arg>(arg)], std::forward<Rest>(rest)...);
    }
}

template <class Out, class... Args>
void errors(const string& yaml, const string& message, Args... args) {
    genny::Orchestrator orchestrator{};
    string modified = baseYaml() + yaml;
    NodeSource ns{modified, "errors-testcase"};
    auto test = [&]() {
        auto context = WorkloadContext{ns.root(), orchestrator, Cast{}};
        return applyBracket(context, std::forward<Args>(args)...).template to<Out>();
    };
    CHECK_THROWS_WITH(test(), StartsWith(message));
}
template <class Out,
          bool Required = true,
          class OutV = typename std::conditional<Required, Out, std::optional<Out>>::type,
          class... Args>
void gives(const string& yaml, OutV expect, Args... args) {
    genny::Orchestrator orchestrator{};
    string modified = baseYaml() + yaml;
    NodeSource ns{modified, "gives-test"};
    auto test = [&]() {
        auto context = WorkloadContext{ns.root(), orchestrator, Cast{}};
        if constexpr (Required) {
            return applyBracket(context, std::forward<Args>(args)...).template to<Out>();
        } else {
            return applyBracket(context, std::forward<Args>(args)...).template maybe<Out>();
        }
    };
    REQUIRE(test() == expect);
}

struct NopProducer : public ActorProducer {
    NopProducer() : ActorProducer("Nop") {}

    ActorVector produce(ActorContext& context) override {
        return {};
    }
};

struct OpProducer : public ActorProducer {
    explicit OpProducer(std::function<void(ActorContext&)> op)
    : ActorProducer("Op"), _op{std::move(op)} {}

    ActorVector produce(ActorContext& context) override {
        _op(context);
        return {};
    }

    std::function<void(ActorContext&)> _op;
};

TEST_CASE("loads configuration okay") {
    genny::Orchestrator orchestrator{};

    auto cast = Cast{
        {"Nop", std::make_shared<NopProducer>()},
    };

    SECTION("Valid YAML") {
        auto yaml = NodeSource(R"(
SchemaVersion: 2018-07-01
Clients:
  Default:
    URI: 'mongodb://localhost:27017'
Actors:
- Name: HelloWorld
  Type: Nop
  Count: 7
        )",
                               "");

        WorkloadContext w{yaml.root(), orchestrator, cast};
        auto& actors = w["Actors"];
    }

    SECTION("Invalid Schema Version") {
        auto yaml = NodeSource("SchemaVersion: 2018-06-27\nClients: {Default: {URI: 'mongodb://localhost:27017'}}\nActors: []\n", "");

        auto test = [&]() { WorkloadContext w(yaml.root(), orchestrator, cast); };
        REQUIRE_THROWS_WITH(test(), Matches("Invalid Schema Version: 2018-06-27"));
    }


    SECTION("Can Construct RNG") {
        std::atomic_int calls = 0;
        auto foobar = genny::testing::toDocumentBson("foo: bar");

        auto fromDocListAssert = false;
        auto fromDocList = std::make_shared<OpProducer>([&](ActorContext& a) {
            for (const auto&& [k, doc] : a["docs"]) {
                auto docgen = doc.to<DocumentGenerator>(a, 1);
                fromDocListAssert = docgen().view() == foobar.view();
                ++calls;
            }
        });

        auto fromDocAssert = false;
        auto fromDoc = std::make_shared<OpProducer>([&](ActorContext& a) {
            auto docgen = a["doc"].to<DocumentGenerator>(a, 1);
            fromDocAssert = docgen().view() == foobar.view();
            ++calls;
        });

        auto cast2 = Cast{{{"fromDocList", fromDocList}, {"fromDoc", fromDoc}}};
        auto yaml = NodeSource(
            "SchemaVersion: 2018-07-01\n"
            "Clients:\n"
            "  Default:\n"
            "    URI: 'mongodb://localhost:27017'\n"
            "Actors: [ "
            "  {Type: fromDocList, docs: [{foo: bar}]}, "
            "  {Type: fromDoc,     doc:   {foo: bar}} "
            "]",
            "");

        auto test = [&]() { WorkloadContext w(yaml.root(), orchestrator, cast2); };
        test();

        REQUIRE(fromDocListAssert);
        REQUIRE(fromDocAssert);
        REQUIRE(calls == 2);
    }

    SECTION("Invalid config accesses") {
        // key not found
        errors<string>("Foo: bar", "Invalid key 'FoO'", "FoO");
        // yaml library does type-conversion; we just forward through...
        gives<string>("Foo: 123", "123", "Foo");
        gives<int>("Foo: 123", 123, "Foo");
        // ...and propagate errors.
        errors<int>("Foo: Bar",
                    "Couldn't convert to 'int': 'bad conversion' at (Line:Column)=(3:5). On node "
                    "with path 'errors-testcase/Foo",
                    "Foo");
        // okay
        gives<int>("Foo: [1,\"bar\"]", 1, "Foo", 0);
        // give meaningful error message:
        errors<string>("Foo: [1,\"bar\"]",
                       "Invalid key '0': Tried to access node that doesn't exist. On node with "
                       "path 'errors-testcase/Foo/0': ",
                       "Foo",
                       "0");

        errors<string>("Foo: 7",
                       "Invalid key 'Bar': Tried to access node that doesn't exist. On node with "
                       "path 'errors-testcase/Foo/Bar",
                       "Foo",
                       "Bar");
        errors<string>("Foo: 7",
                       "Invalid key 'Bat': Tried to access node that doesn't exist. On node with "
                       "path 'errors-testcase/Foo/Bar/Baz/Bat': ",
                       "Foo",
                       "Bar",
                       "Baz",
                       "Bat");

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
        auto yaml = NodeSource("", "");
        auto test = [&]() { WorkloadContext w(yaml.root(), orchestrator, cast); };
        REQUIRE_THROWS_WITH(
            test(),
            Matches(
                R"(Invalid key 'SchemaVersion': Tried to access node that doesn't exist. On node with path '/SchemaVersion': )"));
    }
    SECTION("No Actors") {
        auto yaml = NodeSource("SchemaVersion: 2018-07-01\nClients: {Default: {URI: 'mongodb://localhost:27017'}}", "");
        auto test = [&]() { WorkloadContext w(yaml.root(), orchestrator, cast); };
        test();
    }

    SECTION("Can call two actor producers") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Clients:
  Default:
    URI: 'mongodb://localhost:27017'
Actors:
- Name: One
  Type: SomeList
  SomeList: [100, 2, 3]
- Name: Two
  Type: Count
  Count: 7
  SomeList: [2]
        )",
                      "");

        struct SomeListProducer : public ActorProducer {
            using ActorProducer::ActorProducer;

            ActorVector produce(ActorContext& context) override {
                workloadAssert =
                    context.workload()["Actors"][0]["SomeList"][0].to<int>() == 100;
                actorAssert = 
                    context["SomeList"][0].to<int>() == 100;
                ++calls;
                return ActorVector{};
            }

            int calls = 0;

            // Because Catch2 isn't thread-safe.
            bool workloadAssert = false;
            bool actorAssert = false;
        };

        struct CountProducer : public ActorProducer {
            using ActorProducer::ActorProducer;

            ActorVector produce(ActorContext& context) override {
                actorAssert = context.workload()["Actors"][1]["Count"].to<int>() == 7;
                workloadAssert = context["Count"].to<int>() == 7;
                ++calls;
                return ActorVector{};
            }

            bool workloadAssert = false;
            bool actorAssert = false;
            int calls = 0;
        };

        auto someListProducer = std::make_shared<SomeListProducer>("SomeList");
        auto countProducer = std::make_shared<CountProducer>("Count");

        auto twoActorCast = Cast{
            {"SomeList", someListProducer},
            {"Count", countProducer},
        };
        auto& yaml = ns.root();

        auto context = WorkloadContext{yaml, orchestrator, twoActorCast};


        REQUIRE(someListProducer->calls == 1);
        REQUIRE(someListProducer->workloadAssert);
        REQUIRE(someListProducer->actorAssert);

        REQUIRE(countProducer->calls == 1);
        REQUIRE(countProducer->workloadAssert);
        REQUIRE(countProducer->actorAssert);
        REQUIRE(std::distance(context.actors().begin(), context.actors().end()) == 0);
    }

    SECTION("Will throw if Producer is defined again") {
        auto testFun = []() {
            auto nopProducer = std::make_shared<NopProducer>();
            auto cast = Cast{
                {"Foo", nopProducer},
                {"Bar", nopProducer},
                {"Foo", nopProducer},
            };
        };
        REQUIRE_THROWS_WITH(testFun(), StartsWith(R"(Failed to add 'Nop' as 'Foo')"));
    }
}

void onContext(const NodeSource& yaml, const std::function<void(ActorContext&)>& op) {
    genny::Orchestrator orchestrator{};

    auto cast = Cast{
        {"Op", std::make_shared<OpProducer>(op)},
        {"Nop", std::make_shared<NopProducer>()},
    };

    WorkloadContext{yaml.root(), orchestrator, cast};
}

TEST_CASE("PhaseContexts constructed as expected") {
    NodeSource ns(R"(
    SchemaVersion: 2018-07-01
    Clients:
      Default:
        URI: 'mongodb://localhost:27017'
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
      - Operation: Four
        Phase: 3..5
      - Operation: Five
        Phase: 6..7
        Foo2: Bar3
    )",
                  "");

    SECTION("Loads Phases") {
        // "test of the test"
        int calls = 0;
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) { ++calls; };
        onContext(ns, op);
        REQUIRE(calls == 1);
    }

    SECTION("One Phase per block") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            const auto& ph = ctx.phases();
            REQUIRE(ph.size() == 8);
        };
        onContext(ns, op);
    }
    SECTION("Phase index is defaulted") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE((*ctx.phases().at(0))["Operation"].to<std::string>() == "One");
            REQUIRE((*ctx.phases().at(1))["Operation"].to<std::string>() == "Three");
            REQUIRE((*ctx.phases().at(2))["Operation"].to<std::string>() == "Two");
            REQUIRE((*ctx.phases().at(3))["Operation"].to<std::string>() == "Four");
            REQUIRE((*ctx.phases().at(4))["Operation"].to<std::string>() == "Four");
            REQUIRE((*ctx.phases().at(5))["Operation"].to<std::string>() == "Four");
            REQUIRE((*ctx.phases().at(6))["Operation"].to<std::string>() == "Five");
            REQUIRE((*ctx.phases().at(7))["Operation"].to<std::string>() == "Five");
        };
        onContext(ns, op);
    }
    SECTION("Phases can have extra configs") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE((*ctx.phases().at(1))["Extra"][0].to<int>() == 1);
        };
        onContext(ns, op);
    }
    SECTION("Missing require values throw") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE_THROWS((*ctx.phases().at(1))["Extra"]["100"].to<int>());
        };
        onContext(ns, op);
    }
}

TEST_CASE("Duplicate Phase Numbers") {
    genny::Orchestrator orchestrator{};

    auto cast = Cast{
        {"Nop", std::make_shared<NopProducer>()},
    };

    SECTION("Phase Number syntax") {
        NodeSource ns(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: 'mongodb://localhost:27017'
        Actors:
        - Type: Nop
          Phases:
          - Phase: 0
          - Phase: 0
        )",
                      "");
        auto& yaml = ns.root();

        REQUIRE_THROWS_WITH((WorkloadContext{yaml, orchestrator, cast}),
                            Catch::Matches("Duplicate phase 0"));
    }

    SECTION("PhaseRange syntax") {
        NodeSource ns(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: 'mongodb://localhost:27017'
        Actors:
        - Type: Nop
          Phases:
          - Phase: 0
          - Phase: 0..11
        )",
                      "");
        auto& yaml = ns.root();

        REQUIRE_THROWS_WITH((WorkloadContext{yaml, orchestrator, cast}),
                            Catch::Matches("Duplicate phase 0"));
    }
}

TEST_CASE("No PhaseContexts") {
    NodeSource ns(R"(
    SchemaVersion: 2018-07-01
    Clients:
      Default:
        URI: 'mongodb://localhost:27017'
    Actors:
    - Name: HelloWorld
      Type: Nop
    )",
                  "");

    SECTION("Empty PhaseContexts") {
        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().empty());
        };
        onContext(ns, op);
    }
}

TEST_CASE("PhaseContexts constructed correctly with PhaseRange syntax") {
    SECTION("One Phase per block") {
        auto yaml = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: 'mongodb://localhost:27017'
        Actors:
        - Name: HelloWorld
          Type: Nop
        Phases:
        - Phase: 0
        - Phase: 1..4
        - Phase: 5..5
        - Phase: 6
        - Phase: 7..1e1
        - Phase: 11..11
        - Phase: 12
        )",
                               "");

        std::function<void(ActorContext&)> op = [&](ActorContext& ctx) {
            REQUIRE(ctx.phases().size() == 13);
        };
        onContext(yaml, op);
    }
}

TEST_CASE("Actors Share WorkloadContext State") {

    struct PhaseConfig {
        explicit PhaseConfig(PhaseContext& ctx) {}
    };

    class DummyInsert : public Actor {
    public:
        struct InsertCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

        explicit DummyInsert(ActorContext& actorContext)
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
        explicit DummyFind(ActorContext& actorContext)
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

    NodeSource ns(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: 'mongodb://localhost:27017'
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
    )",
                  "");
    auto& config = ns.root();

    ActorHelper ah(config, 20, {{"DummyInsert", insertProducer}, {"DummyFind", findProducer}});
    ah.run();

    REQUIRE(WorkloadContext::getActorSharedState<DummyInsert, DummyInsert::InsertCounter>() ==
            10 * 10);
}

struct TakesInt {
    int value;
    explicit TakesInt(int x) : value{x} {
        if (x > 7) {
            throw std::logic_error("Expected");
        }
    }
};

struct AnotherInt : public TakesInt {
    // need no-arg ctor to be able to do YAML::Node::as<>()
    AnotherInt() : AnotherInt(0) {}
    explicit AnotherInt(int x) : TakesInt(x) {}
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

// This test is slightly duplicated in context_test.cpp
TEST_CASE("context getPlural") {
    auto createYaml = [](const std::string& actorYaml) -> NodeSource {
        auto doc = YAML::Load(R"(
SchemaVersion: 2018-07-01
Clients:
  Default:
    URI: 'mongodb://localhost:27017'
Numbers: [1,2,3]
Actors: [{}]
)");
        auto actor = YAML::Load(actorYaml);
        actor["Type"] = "Op";
        doc["Actors"][0] = actor;
        return NodeSource{YAML::Dump(doc), ""};
    };

    // can use built-in decode types
    onContext(createYaml("Foo: 5"), [](ActorContext& c) {
        auto x = c.getPlural<TakesInt>("Foo", "Foos", [](const Node& n) { return TakesInt{n.to<int>()}; });
        REQUIRE(!x.empty());
    });

    onContext(createYaml("Foo: 5"), [](ActorContext& c) {
        REQUIRE(c.getPlural<AnotherInt>("Foo", "Foos")[0].value == 5);
    });

    onContext(createYaml("{}"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH(
            [&]() { auto x = c.getPlural<int>("Foo", "Foos"); }(),
            Matches("Invalid key 'getPlural\\('Foo', 'Foos'\\)': Either 'Foo' or 'Foos' required. "
                    "On node with path '/Actors/0': \\{Type: Op\\}"));
    });
    onContext(createYaml("Foo: 81"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH(
            [&]() {
                auto x = c.getPlural<TakesInt>(
                    "Foo", "Foos", [](const Node& n) { return TakesInt{n.to<int>()}; });
                FAIL("unreachable");
            }(),
            Matches("Expected"));
    });

    onContext(createYaml("Foos: [733]"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Foo", "Foos") == std::vector<int>{733});
    });

    onContext(createYaml("Foos: 73"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH(
            [&]() { auto x = c.getPlural<int>("Foo", "Foos"); }(),
            Matches("Invalid key 'getPlural\\('Foo', 'Foos'\\)': Plural 'Foos' must be a sequence "
                    "type. On node with path '/Actors/0': \\{Foos: 73, Type: Op\\}"));
    });

    onContext(createYaml("Foo: 71"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Foo", "Foos") == std::vector<int>{71});
    });

    onContext(createYaml("{ Foo: 9, Foos: 1 }"), [](ActorContext& c) {
        REQUIRE_THROWS_WITH(
            [&]() { auto x = c.getPlural<int>("Foo", "Foos"); }(),
            Matches("Invalid key 'getPlural\\('Foo', 'Foos'\\)': Can't have both 'Foo' and 'Foos'. "
                    "On node with path '/Actors/0': \\{Foo: 9, Foos: 1, Type: Op\\}"));
    });

    onContext(createYaml("Numbers: [3, 4, 5]"), [](ActorContext& c) {
        REQUIRE(c.getPlural<int>("Number", "Numbers") == std::vector<int>{3, 4, 5});
    });
}

TEST_CASE("If no producer exists for an actor, then we should throw an error") {
    genny::Orchestrator orchestrator{};

    auto cast = Cast{
        {"Foo", std::make_shared<NopProducer>()},
    };

    auto yaml = NodeSource(R"(
    SchemaVersion: 2018-07-01
    Clients:
      Default:
        URI: 'mongodb://localhost:27017'
    Database: test
    Actors:
    - Name: Actor1
      Type: Bar
    )",
                           "");

    SECTION("Incorrect type value inputted") {
        auto test = [&]() { WorkloadContext w(yaml.root(), orchestrator, cast); };
        REQUIRE_THROWS_WITH(
            test(), Catch::Contains("Unable to construct actors: No producer for 'Bar'"));
    }
}
