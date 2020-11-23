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
  - Duration: 10 milliseconds
- Name: 1
  Type: LoggingActor
  Threads: 1
  Phases:
  - LogEvery: 3 milliseconds
    Blocking: None
Metrics:
  Format: cedar-csv
  Path: build/genny-metrics
)",
                          ""};
        ActorHelper ah(config.root(), 2, MongoTestFixture::connectionUri().to_string());
        ah.run();
        // Don't actually assert anything because it's hard to assert interactions with logging.
        // If you're running this test manually you should see exactly 3 log messages from
        // the LoggingActor.
    }
}
}  // namespace
