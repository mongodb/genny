#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/config.hpp>
#include <gennylib/metrics.hpp>
#include <gennylib/ErrorBag.hpp>

namespace {

template<typename...Str>
std::string errString(Str&&...args) {
    auto strs = std::vector<std::string>{ std::forward<Str>(args)... };
    std::stringstream out;
    for(const auto& str : strs) {
        out << u8"😱 " << str;
    }
    return out.str();
}

std::string report(genny::ErrorBag& bag) {
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
        REQUIRE(report(errors) == "");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27");
        genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errors};
        REQUIRE((bool)errors);
        REQUIRE(report(errors) == errString("Key SchemaVersion expect [2018-07-01] but is [2018-06-27]"));
    }


}