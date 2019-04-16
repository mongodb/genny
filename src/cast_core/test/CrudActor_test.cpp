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

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace BasicBson = bsoncxx::builder::basic;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture,
                 "Test 'count_documents' operation.",
                 "[standalone][single_node_replset][three_node_replset][CrudActor]") {

    dropAllDatabases();
    auto events = ApmEvents{};
    auto db = client.database("mydb");

    SECTION("Perform a count_documents on the collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operation:
                OperationName: countDocuments
                OperationCommand:
                  Filter: { a: 1 }
          )");
        try {
            auto apmCallback = makeApmCallback(events);
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), apmCallback);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(events.size() > 0);
            auto countEvent = events[0].command;
            auto collection = countEvent["aggregate"].get_utf8().value;
            REQUIRE(std::string(collection) == "test");

            REQUIRE(countEvent["pipeline"][0]["$match"].get_document().view() ==
                    BasicBson::make_document(BasicBson::kvp("a", 1)));
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Test write operations.",
                 "[standalone][single_node_replset][three_node_replset][CrudActor]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    SECTION("Insert a document into a collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operation:
                OperationName: insertOne
                OperationCommand:
                  Document: { a: 1 }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            REQUIRE(count == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Insert and replace document in a collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: insertOne
                OperationCommand:
                  Document: { a: 1 }
              - OperationName: replaceOne
                OperationCommand:
                  Filter: { a : 1 }
                  Replacement: { newfile: test }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto countOldDoc = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            auto countNew = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("newfile", "test")));
            REQUIRE(countOldDoc == 0);
            REQUIRE(countNew == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Insert and update document in a collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: insertOne
                OperationCommand:
                  Document: { a: 1 }
              - OperationName: updateOne
                OperationCommand:
                  Filter: { a: 1 }
                  Update: { $set: { a: 10 } }   
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto countOldDoc = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            auto countUpdated = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 10)));
            REQUIRE(countOldDoc == 0);
            REQUIRE(countUpdated == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Insert and update multiple documents in a collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: insertOne
                OperationCommand:
                  Document: { a: {^RandomInt: {min: 5, max: 15} } }
              - OperationName: insertOne
                OperationCommand:
                  Document: { a: {^RandomInt: {min: 5, max: 15} } }
              - OperationName: updateMany
                OperationCommand:
                  Filter: { a: { $gte: 5 } }
                  Update: { $set: { a: 2 } }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto countOldDocs = db.collection("test").count_documents(BasicBson::make_document(
                BasicBson::kvp("a", BasicBson::make_document(BasicBson::kvp("$gte", 5)))));
            auto countUpdated = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 2)));
            REQUIRE(countOldDocs == 0);
            REQUIRE(countUpdated == 2);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Delete multiple documents in a collection.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: deleteMany
                OperationCommand:
                  Filter: { a: 1 }
          )");
        try {
            db.collection("test").insert_one(BasicBson::make_document(BasicBson::kvp("a", 1)));
            db.collection("test").insert_one(BasicBson::make_document(BasicBson::kvp("a", 1)));
            auto count = db.collection("test").count_documents(BasicBson::make_document());
            REQUIRE(count == 2);
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            REQUIRE(count == 0);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

// TODO: add test for ReadConcern
// TODO: add test for Find
// TODO: add test for start and commit transaction.

}  // namespace
