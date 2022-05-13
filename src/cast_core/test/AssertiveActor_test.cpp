// Copyright 2022-present MongoDB Inc.
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

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture, "AssertiveActor passes an assert", "[standalone][single_node_replset][three_node_replset][sharded][AssertiveActor]") {

    dropAllDatabases();
    auto db = client.database("test");
  
    // The test collection is empty, so this should trivially pass.
    NodeSource passNodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
        Actors:
        - Name: PassAssertiveActor
          Type: AssertiveActor
          Database: test
          Phases:
          - Repeat: 1
            Expected:
              aggregate: test
              pipeline: []
              cursor: {batchSize: 101}
            Actual:
              aggregate: test
              pipeline: []
              cursor: {batchSize: 101}
    )", __FILE__);

    SECTION("Assert passes") {
        try {
            genny::ActorHelper ah(passNodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            INFO("Assert passed")
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            FAIL("Expected assert to pass, but an exception was thrown " << diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture, "AssertiveActor fails an assert", "[standalone][single_node_replset][three_node_replset][sharded][AssertiveActor]") {

    dropAllDatabases();
    auto db = client.database("test");
    auto testColl = db.collection("test");
    testColl.insert_one(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("a", 1)));
    testColl.insert_one(bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("a", 2)));

    // The test collection is not empty, so this should trivially fail.
    NodeSource failNodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
        Actors:
        - Name: FailAssertiveActor
          Type: AssertiveActor
          Database: test
          Phases:
          - Repeat: 1
            Expected:
              aggregate: test
              pipeline: []
              cursor: {batchSize: 101}
            Actual:
              aggregate: emptyColl
              pipeline: []
              cursor: {batchSize: 101}
    )", __FILE__);

    SECTION("Assert fails") {
        try {
            genny::ActorHelper ah(failNodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            FAIL("Expected assert to fail, but no exception was thrown");
        } catch (const std::exception& e) {
            // Exprected assert to reach here.
            auto diagInfo = boost::diagnostic_information(e);
            INFO("Assert failed correctly" << diagInfo);
        }
    }
}

}  // namespace
}  // namespace genny
