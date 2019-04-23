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

TEST_CASE("WorkloadParser can run generate smoke test configurations") {
    const auto input = (R"(
Actors:
- Name: WorkloadParserTest
  Type: NonExistent
  Threads: 2.718281828   # This field is ignored for the purpose of this test.
  Foo:
    Repeat: "do-not-touch"
  Phases:
  - Duration: 4 scores        # Removed
    Repeat: 1e999             # Replaced with "1"
    Rate: 1 per 2 megannum    # Removed
    SleepBefore: 2 planks     # Removed
    SleepAfter: 1 longtime    # Removed
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
