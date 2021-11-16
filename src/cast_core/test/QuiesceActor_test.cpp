// Copyright 2021-present MongoDB Inc.
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

// Enable in TIG-3548
/**
#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;

TEST_CASE_METHOD(
    MongoTestFixture,
    "QuiesceActor",
    "[standalone][single_node_replset][three_node_replset][sharded][QuiesceActor]") {

    NodeSource config = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: QuiesceActor
          Type: QuiesceActor
          # Using multiple threads will result in an error.
          Threads: 1
          Database: mydb
          Phases:
          - Phase: 0
            Repeat: 1
          - Phase: 1
            Repeat: 1
    )",
                                   __FILE__);


    SECTION("Quiesce collection") {
        dropAllDatabases();
        auto db = client.database("mydb");
        ActorHelper ah(config.root(), 1, MongoTestFixture::connectionUri().to_string());
        // We just check that quiescing the cluster doesn't crash it.
        ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
    }
}

}  // namespace
}  // namespace genny
*/
