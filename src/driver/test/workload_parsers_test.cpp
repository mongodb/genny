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

#include <driver/workload_parsers.hpp>

#include <testlib/helpers.hpp>

namespace genny::driver::v1 {
namespace {

TEST_CASE("Contexts have scope") {
    Context* context = nullptr;
    ContextGuard outerContext(context);

    YAML::Node outer;
    outer["outerKey"] = "outerVal";
    context->insert("outerName", outer, Type::kParameter);

    {
        ContextGuard innerContext(context);
        YAML::Node inner;
        inner["innerKey1"] = "innerVal1";
        context->insert("innerName1", inner, Type::kParameter);

        auto retrievedOuter = context->get("outerName", Type::kParameter);
        REQUIRE(retrievedOuter == outer);

        auto retrievedInner = context->get("innerName1", Type::kParameter);
        REQUIRE(retrievedInner == inner);
    }

    {
        ContextGuard innerContext(context);
        YAML::Node inner;
        inner["innerKey2"] = "innerVal2";
        context->insert("innerName2", inner, Type::kParameter);

        auto retrievedOuter = context->get("outerName", Type::kParameter);
        REQUIRE(retrievedOuter == outer);

        auto retrievedInner = context->get("innerName2", Type::kParameter);
        REQUIRE(retrievedInner == inner);

        auto retrievedOldInner = context->get("innerName1", Type::kParameter);
        REQUIRE_FALSE(retrievedOldInner == inner);

    }

    auto retrievedOuter = context->get("outerName", Type::kParameter);
    REQUIRE(retrievedOuter == outer);

    REQUIRE_THROWS(context->get("outerName", Type::kActorTemplate));

}

TEST_CASE("WorkloadParser can run generate smoke test configurations") {
    const auto input = (R"(
Actors:
- Name: WorkloadParserTest
  Type: NonExistent
  Threads: 2.718281828   # This field is ignored for the purpose of this test.
  Foo:
    Repeat: "do-not-touch"
  Phases:
  - Duration: 4 scores            # Removed
    Repeat: 1e999                 # Replaced with "1"
    GlobalRate: 1 per 2 megannum  # Removed
    SleepBefore: 2 planks         # Removed
    SleepAfter: 1 longtime        # Removed
    Bar:
      Duration: "do-not-touch"
)");

    const auto expected = (R"(Actors:
  - Name: WorkloadParserTest
    Type: NonExistent
    Threads: 2.718281828
    Foo:
      Repeat: do-not-touch
    Phases:
      - Repeat: 1
        Bar:
          Duration: do-not-touch)");

    auto cwd = boost::filesystem::current_path();

    WorkloadParser p{cwd};
    auto parsedConfig = p.parse(input, DefaultDriver::ProgramOptions::YamlSource::kString);
    parsedConfig = SmokeTestConverter::convert(parsedConfig);

    REQUIRE(YAML::Dump(parsedConfig) == expected);
}

}  // namespace
}  // namespace genny::driver::v1
