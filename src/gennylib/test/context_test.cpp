#include "test.h"

#include <iomanip>
#include <iostream>

#include <yaml-cpp/yaml.h>

#include <gennylib/ErrorBag.hpp>
#include <gennylib/context.hpp>
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
        auto workloadContext = WorkloadContext{yaml, metrics, orchestrator, {}};
        REQUIRE(!workloadContext.errors());
        REQUIRE(reported(workloadContext.errors()) == "");
    }

    SECTION("Invalid Schema Version") {
        auto yaml = YAML::Load("SchemaVersion: 2018-06-27");
        auto result = WorkloadContext{yaml, metrics, orchestrator, {}};
        REQUIRE((bool)result.errors());
        REQUIRE(reported(result.errors()) ==
                errString("Key SchemaVersion expect [2018-07-01] but is [2018-06-27]"));
    }

    SECTION("Empty Yaml") {
        auto yaml = YAML::Load("");
        auto result = WorkloadContext{yaml, metrics, orchestrator, {}};
        REQUIRE((bool)result.errors());
        REQUIRE(reported(result.errors()) == errString("Key SchemaVersion not found"));
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

        int calls = 0;
        std::vector<WorkloadContext::Producer> producers;
        producers.emplace_back([&](ActorContext& actoractorContext) {
            // purposefully "fail" require
            actoractorContext.require("Name", std::string("One"));
            actoractorContext.require("Count", 5);  // we're type-safe
            actoractorContext.require(actoractorContext["SomeList"], 0, 100);
            ++calls;
            return WorkloadContext::ActorVector{};
        });
        producers.emplace_back([&](ActorContext&) {
            ++calls;
            return WorkloadContext::ActorVector{};
        });

        auto actors = WorkloadContext{yaml, metrics, orchestrator, producers};

        REQUIRE(reported(actors.errors()) ==
                errString("Key Count not found",
                          "Key Name expect [One] but is [Two]",
                          "Key Count expect [5] but is [7]",
                          "Key 0 expect [100] but is [2]"));
        REQUIRE(calls == 4);
    }
}