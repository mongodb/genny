#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/ErrorBag.hpp>
#include <gennylib/config.hpp>
#include <gennylib/metrics.hpp>

namespace {

template <typename... Str>
std::string errString(Str&&... args) {
    auto strs = std::vector<std::string>{std::forward<Str>(args)...};
    std::stringstream out;
    for (const auto& str : strs) {
        out << u8"😱 " << str;
    }
    return out.str();
}

std::string reported(const genny::ErrorBag& bag) {
    std::stringstream out;
    bag.report(out);
    return out.str();
}

}  // namespace

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
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator};
        auto result = factory.actors();
        REQUIRE(!result.errorBag);
        REQUIRE(reported(result.errorBag) == "");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator};
        auto result = factory.actors();
        REQUIRE((bool)result.errorBag);
        REQUIRE(reported(result.errorBag) ==
                errString("Key SchemaVersion expect [2018-07-01] but is [2018-06-27]"));
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator};
        auto result = factory.actors();
        REQUIRE((bool)result.errorBag);
        REQUIRE(reported(result.errorBag) == errString("Key SchemaVersion not found"));
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
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator};

        int calls = 0;
        factory.addProducer([&](ActorConfig& actorConfig) {
            // purposefully "fail" require
            actorConfig.require("Name", std::string("One"));
            actorConfig.require("Count", 5); // we're type-safe
            actorConfig.require(actorConfig["SomeList"], 0, 100);
            ++calls;
            return PhasedActorFactory::ActorVector {};
        });
        factory.addProducer([&](ActorConfig& actorConfig) {
            ++calls;
            return PhasedActorFactory::ActorVector {};
        });

        auto actors = factory.actors();

        REQUIRE(reported(actors.errorBag) == errString(
            "Key Count not found",
            "Key Name expect [One] but is [Two]",
            "Key Count expect [5] but is [7]",
            "Key 0 expect [100] but is [2]"
        ));
        REQUIRE(calls == 4);
    }
}