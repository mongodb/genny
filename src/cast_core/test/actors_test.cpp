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

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

namespace genny {
namespace {

using namespace genny::testing;

namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture,
                 "Successfully connects to a MongoDB instance.",
                 "[single_node_replset][three_node_replset][sharded]") {

    dropAllDatabases();
    auto db = client.database("test");

    auto builder = bson_stream::document{};
    bsoncxx::document::value doc_value = builder
        << "name"
        << "MongoDB"
        << "type"
        << "database"
        << "count" << 1 << "info" << bson_stream::open_document << "x" << 203 << "y" << 102
        << bson_stream::close_document << bson_stream::finalize;

    SECTION("Insert a document into the database.") {

        bsoncxx::document::view view = doc_value.view();
        db.collection("test").insert_one(view);

        REQUIRE(db.collection("test").count_documents(view) == 1);
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Pre-warming a client works, by sending a ping to the DB.",
                 "[sharded][single_node_replset][three_node_replset]") {

    SECTION("Pre-warming is enabled by default, so must ping.") {
        auto session = MongoTestFixture::client.start_session();
        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        NodeSource config(R"(
            SchemaVersion: 2018-07-01
            Clients:
              Default:
                URI: )" + MongoTestFixture::connectionUri().to_string() +
                              R"(
            Actors:
              - Name: TestActor
                Type: RunCommand
                Threads: 1
                Phases:
                - {Nop: true}
            Metrics:
              Format: csv
        )",
                          "");
        ActorHelper ah{
            config.root(), 1, apmCallback};
        ah.run();
        REQUIRE(events.size() == 1);
        auto&& ping_event = events.front();
        REQUIRE(ping_event.command_name == "ping");
    }

    SECTION("Pre-warming is disabled, so no ping.") {
        auto session = MongoTestFixture::client.start_session();
        auto events = ApmEvents{};
        auto apmCallback = makeApmCallback(events);
        NodeSource config(R"(
            SchemaVersion: 2018-07-01
            Clients:
              Default:
                NoPreWarm: true
                URI: )" + MongoTestFixture::connectionUri().to_string() +
                              R"(
            Actors:
              - Name: TestActor
                Type: RunCommand
                Threads: 1
                Phases:
                - {Nop: true}
            Metrics:
              Format: csv
        )",
                          "");
        ActorHelper ah{
            config.root(), 1, apmCallback};
        ah.run();
        REQUIRE(events.size() == 0);
    }
}

}  // namespace
}  // namespace genny
