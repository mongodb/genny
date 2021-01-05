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

TEST_CASE("WorkloadParser parameters are scoped") {
    const auto input = (R"(
ActorTemplates:
- TemplateName: TestTemplate1
  Config:
    Name: {^Parameter: {Name: "Name", Default: "DefaultValue"}}
    SomeKey: SomeValue
    Phases:
      OnlyIn:
        Active: [{^Parameter: {Name: "Phase", Default: 1}}]
        Max: 3
        Config:
          Duration: {^Parameter: {Name: "Duration", Default: 3 minutes}}

- TemplateName: TestTemplate2
  Config:
    Name: {^Parameter: {Name: "Name", Default: "DefaultValue"}}
    SomeKey: SomeValue
    Phases:
      - Nop: true
      - Nop: true
      - ExternalPhaseConfig:
          Path: src/testlib/phases/Good.yml
          Parameters:
            Repeat: 2
      - Nop: true
    AnotherValueFromRepeat: {^Parameter: {Name: "Repeat", Default: "BadDefault"}}

Actors:
- ActorFromTemplate:
    TemplateName: TestTemplate1
    TemplateParameters:
      Name: ActorName1
      Phase: 0
      Duration: 5 minutes

# Lacking the specified duration, we expect the default duration to be used,
# instead of the one from the previous ActorFromTemplate which was scoped to that block.
- ActorFromTemplate:
    TemplateName: TestTemplate1
    TemplateParameters:
      Phase: 1
      Name: ActorName2

# The value of Repeat should be correctly "shadowed" in the lower level external phase.
- ActorFromTemplate:
    TemplateName: TestTemplate2
    TemplateParameters:
      Name: ActorName3
      Repeat: GoodValue
)");

    const auto expected = (R"(Actors:
  - Name: ActorName1
    SomeKey: &1 SomeValue
    Phases:
      - Duration: 5 minutes
      - &2
        Nop: true
      - *2
      - *2
  - Name: ActorName2
    SomeKey: *1
    Phases:
      - &3
        Nop: true
      - Duration: 3 minutes
      - *3
      - *3
  - Name: ActorName3
    SomeKey: SomeValue
    Phases:
      - Nop: true
      - Nop: true
      - Repeat: 2
        Mode: NoException
      - Nop: true
    AnotherValueFromRepeat: GoodValue)");

    auto cwd = boost::filesystem::current_path();

    WorkloadParser p{cwd};
    auto parsedConfig = p.parse(input, DefaultDriver::ProgramOptions::YamlSource::kString);

    REQUIRE(YAML::Dump(parsedConfig) == expected);
}


TEST_CASE("WorkloadParser can preprocess keywords") {
    const auto input = (R"(
ActorTemplates:
- TemplateName: TestTemplate
  Config:
    Name: {^Parameter: {Name: "Name", Default: "IncorrectDefault"}}
    SomeKey: SomeValue
    Phases:
      OnlyIn:
        Active: [{^Parameter: {Name: "Phase", Default: 1}}]
        Max: 3
        Config:
          Duration: {^Parameter: {Name: "Duration", Default: 3 minutes}}
Actors:
- ActorFromTemplate:
    TemplateName: TestTemplate
    TemplateParameters:
      Name: ActorName1
      Phase: 0
      Duration: 5 minutes
- ActorFromTemplate:
    TemplateName: TestTemplate
    TemplateParameters:
      Phase: 1
      Name: ActorName2
)");

    const auto expected = (R"(Actors:
  - Name: ActorName1
    SomeKey: &1 SomeValue
    Phases:
      - Duration: 5 minutes
      - &2
        Nop: true
      - *2
      - *2
  - Name: ActorName2
    SomeKey: *1
    Phases:
      - &3
        Nop: true
      - Duration: 3 minutes
      - *3
      - *3)");

    auto cwd = boost::filesystem::current_path();

    WorkloadParser p{cwd};
    auto parsedConfig = p.parse(input, DefaultDriver::ProgramOptions::YamlSource::kString);

    REQUIRE(YAML::Dump(parsedConfig) == expected);
}


}  // namespace
}  // namespace genny::driver::v1
