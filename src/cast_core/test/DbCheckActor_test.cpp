// Copyright 2023-present MongoDB Inc.
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


TEST_CASE_METHOD(MongoTestFixture, "DbCheckActor.",
          "[single_node_replset][three_node_replset][DbCheckActor]") {

    dropAllDatabases();
    auto db = client.database("mydb");
    auto local = client.database("local");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
        Actors:
        - Name: DbCheckActor
          Type: DbCheckActor
          Database: mydb
          Threads: 1
          Phases:
          - Repeat: 1
            Collection: mycoll
    )", __FILE__);


    SECTION("Runs successfully.") {
        try {
            // Create the collection against which dbcheck will be executed.
            db.collection("mycoll").insert_one({});
            genny::ActorHelper ah(nodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            // Confirm that the execution of dbcheck was successful.
            auto builder = bson_stream::document{};
            builder << "operation"
                    << "dbCheckStop" << bsoncxx::builder::stream::finalize;
            auto count = local.collection("system.healthlog").count_documents(builder.view());
            REQUIRE(count > 0);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
