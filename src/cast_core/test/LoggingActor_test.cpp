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

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny;
using namespace genny::testing;

TEST_CASE("LoggingActor") {
    SECTION("Configuration") {
        NodeSource config{R"(
SchemaVersion: 2018-07-01
Actors:
- Name: Nop
  Type: NopMetrics
  Phases:
  - Duration: 20 milliseconds
- Name: 1
  Type: LoggingActor
  Phases:
  - LogEvery: 5 milliseconds
    Blocking: None
)",
                          ""};
        ActorHelper ah(config.root(), 2, MongoTestFixture::connectionUri().to_string());
        ah.run();
    }
}
}  // namespace