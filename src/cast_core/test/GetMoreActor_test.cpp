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

#include <bsoncxx/builder/basic/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;

TEST_CASE_METHOD(MongoTestFixture,
                 "GetMoreActor supports running find command",
                 "[standalone][single_node_replset][three_node_replset][sharded][GetMoreActor]") {

    NodeSource config = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: GetMoreActor
          Type: GetMoreActor
          Phases:
          - Repeat: 1
            Database: mydb
            InitialCursorCommand:
              find: mycoll
    )",
                                   __FILE__);


    SECTION("Succeeds when collection is empty") {
        dropAllDatabases();

        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        genny::ActorHelper ah(
            config.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);

        ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

        REQUIRE(events.size() == 1U);
        REQUIRE(events[0].command_name == "find");
    }
}

}  // namespace
}  // namespace genny
