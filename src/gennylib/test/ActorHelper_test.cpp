#include "test.h"

#include <memory.h>

#include <yaml-cpp/yaml.h>

#include <gennylib/ActorProducer.hpp>
#include <gennylib/Actor.hpp>
#include <gennylib/context.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include "log.hh"

#include <ActorHelper.hpp>

using Catch::Matchers::Matches;

TEST_CASE("Actor Helper") {
    class DummyActor : public genny::Actor {
    public:
        DummyActor(genny::ActorContext& ac) : Actor(ac){};

        void run() override {
            BOOST_LOG_TRIVIAL(info) << "In the run method of DummyActor";
        };

        static std::string_view defaultName() {
            return "DummyActor";
        }
    };

    SECTION("Barfs on invalid YAML") {
        YAML::Node badConfig = YAML::Load("{i-am-json-not-yaml}");
        auto dummyProducer = std::make_shared<genny::DefaultActorProducer<DummyActor>>("DummyActor");

        auto test = [&](){genny::ActorHelper ah(badConfig, 1, {{"DummyActor", dummyProducer}});};
        REQUIRE_THROWS_WITH(test(), Matches(R"(Invalid key.*i-am-json-not-yaml.*)"));
    }

    SECTION("Barfs on invalid thread count (aka token)") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)");
        auto dummyProducer = std::make_shared<genny::DefaultActorProducer<DummyActor>>("DummyActor");

        auto test = [&](){genny::ActorHelper ah(config, -1, {{"DummyActor", dummyProducer}});};
        REQUIRE_THROWS_WITH(test(), Matches("Must add a positive number of tokens"));
    }

    class CtorThrowingActor : public DummyActor {
    public:
        CtorThrowingActor(genny::ActorContext& ac) : DummyActor(ac) {
            throw genny::InvalidConfigurationException("CTOR Barf");
        };
    };

    SECTION("Barfs if Actor Ctor barfs") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)");
        auto dummyProducer = std::make_shared<genny::DefaultActorProducer<CtorThrowingActor>>("DummyActor");

        auto test = [&](){genny::ActorHelper ah(config, 1, {{"DummyActor", dummyProducer}});};
        REQUIRE_THROWS_WITH(test(), Matches("CTOR Barf"));
    }

    SECTION("Barfs if Actor runAndVerify() barfs") {
        YAML::Node config = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)");
        auto dummyProducer =
            std::make_shared<genny::DefaultActorProducer<DummyActor>>("DummyActor");

        genny::ActorHelper ah(config, 1, {{"DummyActor", dummyProducer}});
        auto runFunc = [](const genny::WorkloadContext& wc) {};
        auto verifyFunc = [](const genny::WorkloadContext& wc) {
            throw genny::InvalidConfigurationException("RUN Barf");
        };
        REQUIRE_THROWS_WITH(ah.runAndVerify(runFunc, verifyFunc), Matches("RUN Barf"));
    }
}


