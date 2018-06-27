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
    genny::ErrorBag errors;

    SECTION("Valid YAML") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: HelloWorld
  Count: 7
        )");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errors};
        REQUIRE(!errors);
        REQUIRE(reported(errors) == "");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errors};
        REQUIRE((bool)errors);
        REQUIRE(reported(errors) ==
                errString("Key SchemaVersion expect [2018-07-01] but is [2018-06-27]"));
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errors};
        REQUIRE((bool)errors);
        REQUIRE(reported(errors) == errString("Key SchemaVersion not found"));
    }


    SECTION(
        "Two ActorProducers can see all Actors blocks and producers continue even if errors "
        "reported") {
        auto yaml = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
- Name: Two
        )");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errors};

        int calls = 0;
        factory.addProducer([&](const ActorConfig* actorConfig,
                                ErrorBag* errorBag) -> PhasedActorFactory::ActorVector {
            // purposefully "fail" require
            errorBag->require(
                "Name", actorConfig->get("Name").as<std::string>(), std::string{"One"});
            ++calls;
            return {};
        });
        factory.addProducer([&](const ActorConfig* actorConfig,
                                ErrorBag* errorBag) -> PhasedActorFactory::ActorVector {
            ++calls;
            return {};
        });

        auto actors = factory.actors(&errors);

        REQUIRE(reported(errors) == errString("Key Name expect [One] but is [Two]"));
        REQUIRE(calls == 4);
    }
}