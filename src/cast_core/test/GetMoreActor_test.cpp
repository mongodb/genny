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
#include <bsoncxx/builder/basic/kvp.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace BasicBson = bsoncxx::builder::basic;

void runFirstActorAndReportExceptions(const genny::WorkloadContext& wc) {
    try {
        wc.actors()[0]->run();
    } catch (const boost::exception& ex) {
        auto diagInfo = boost::diagnostic_information(ex);
        FAIL(diagInfo);
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "GetMoreActor",
                 "[single_node_replset][three_node_replset][sharded][GetMoreActor]") {

    dropAllDatabases();

    auto db = client.database("mydb");
    auto collection = db.collection("mycoll");
    for (int i = 0; i < 4; ++i) {
        collection.insert_one(BasicBson::make_document());
    }

    SECTION("Will retrieve batches until cursor is exhausted") {
        NodeSource yaml = NodeSource(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: GetMoreActor_MultipleBatches
              Type: GetMoreActor
              Phases:
              - Repeat: 1
                Database: mydb
                InitialCursorCommand:
                  find: mycoll
                  batchSize: 1
                GetMoreBatchSize: 2
        )",
                                     __FILE__);

        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        genny::ActorHelper ah(
            yaml.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);

        ah.run(runFirstActorAndReportExceptions);

        REQUIRE(events.size() == 3U);
        REQUIRE(events[0].command_name == "find");
        REQUIRE(events[1].command_name == "getMore");
        REQUIRE(events[2].command_name == "getMore");
    }

    SECTION("Can omit GetMoreBatchSize") {
        NodeSource yaml = NodeSource(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: GetMoreActor_OmitGetMoreBatchSize
              Type: GetMoreActor
              Phases:
              - Repeat: 1
                Database: mydb
                InitialCursorCommand:
                  find: mycoll
                  batchSize: 0
        )",
                                     __FILE__);

        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        genny::ActorHelper ah(
            yaml.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);

        ah.run(runFirstActorAndReportExceptions);

        REQUIRE(events.size() == 2U);
        REQUIRE(events[0].command_name == "find");
        REQUIRE(events[1].command_name == "getMore");
    }

    SECTION("Can use generators in InitialCursorCommand") {
        NodeSource yaml = NodeSource(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: GetMoreActor_UseGenerator
              Type: GetMoreActor
              Phases:
              - Repeat: 1
                Database: mydb
                InitialCursorCommand:
                  find: mycoll
                  filter: {x: {^RandomInt: {min: 3, max: 3}}}
        )",
                                     __FILE__);

        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        genny::ActorHelper ah(
            yaml.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);

        ah.run(runFirstActorAndReportExceptions);

        REQUIRE(events.size() == 1U);
        REQUIRE(events[0].command_name == "find");

        INFO("command := " << bsoncxx::to_json(events[0].command));

        auto expectedFilter = BasicBson::make_document(BasicBson::kvp("x", int64_t(3)));
        REQUIRE(events[0].command["filter"].get_document().value == expectedFilter);
    }

    SECTION("Can use aggregate command") {
        NodeSource yaml = NodeSource(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: GetMoreActor_AggregateCommand
              Type: GetMoreActor
              Phases:
              - Repeat: 1
                Database: mydb
                InitialCursorCommand:
                  aggregate: mycoll
                  pipeline: []
                  cursor:
                    batchSize: 1
                GetMoreBatchSize: 2
        )",
                                     __FILE__);

        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        genny::ActorHelper ah(
            yaml.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);

        ah.run(runFirstActorAndReportExceptions);

        REQUIRE(events.size() == 3U);
        REQUIRE(events[0].command_name == "aggregate");
        REQUIRE(events[1].command_name == "getMore");
        REQUIRE(events[2].command_name == "getMore");
    }
}

}  // namespace
}  // namespace genny
