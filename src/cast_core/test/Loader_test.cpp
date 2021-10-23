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

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>
#include <regex>
#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;
using std::regex_search;
using namespace bsoncxx;

TEST_CASE_METHOD(MongoTestFixture, "Loader - create records and add indexes",
          "[standalone][single_node_replset-1][three_node_replset][sharded][Loader]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: Loader
          Type: Loader
          Threads: 1
          Phases:
          - Repeat: 1
            Database: mydb
            Collection: mycoll
            CollectionCount: 1
            Threads: 1
            DocumentCount: 10000
            BatchSize: 3000
            Document: {
                field1: {^RandomInt: {min: 0, max: 100}},
                field2: {^RandomInt: {min: 0, max: 100}},
                field3: {^RandomInt: {min: 0, max: 100}},
                field4: {^RandomInt: {min: 0, max: 100}},
            }
            Indexes:
            - keys: {field1: 1, field2: 1}
            - keys: {field2: 1}
              options: {name: "a_index", sparse: true}
            - keys: {field1: 1}
              options: {sparse: true}
            - keys: {field3: 1, field4: 1}
              options: {sparse: true}
            - keys: {field3: 4}
              options: {sparse: true, expireAfterSeconds: 3600}

    )",
    __FILE__);


    SECTION("Inserts documents, create indexes and check if indexed and documents are created") {
        try {
            const auto indexRegex = std::regex(".*\\\"firstBatch\\\"\\s:\\s\\[(.*)\\]");
            genny::ActorHelper ah(nodes.root(), 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            auto builder = bson_stream::document{};
            builder << bson_stream::finalize;
            document::value doc = builder << "listIndexes" << "Collection0"
                                  << builder::stream::finalize;
            auto indexes = db.run_command(doc.view());
            auto outputString = to_json(indexes.view());
            std::cmatch matches;
            std::regex_search(const_cast<char*>(outputString.c_str()), matches, indexRegex);
            std::string actual_response(matches[1]);
            auto expected_response = " { \"v\" : 2, \"key\" : { \"_id\" : 1 }, "
                                     "\"name\" : \"_id_\" }, "
                                     "{ \"v\" : 2, \"key\" : { \"field1\" : 1, \"field2\" : 1 }, "
                                     "\"name\" : \"field1field2\" },"
                                     " { \"v\" : 2, \"key\" : { \"field2\" : 1 },"
                                     " \"name\" : \"a_index\", \"sparse\" : true },"
                                     " { \"v\" : 2, \"key\" : { \"field1\" : 1 }, "
                                     "\"name\" : \"field1\", \"sparse\" : true },"
                                     " { \"v\" : 2, \"key\" : { \"field3\" : 1, \"field4\" : 1 }, "
                                     "\"name\" : \"field3field4\", \"sparse\" : true },"
                                     " { \"v\" : 2, \"key\" : { \"field3\" : 4 }, "
                                     "\"name\" : \"field3\", \"sparse\" : true, "
                                     "\"expireAfterSeconds\" : 3600 } ";
            INFO("Actual Response:   " << actual_response);
            INFO("Expected Response: " << expected_response);
            REQUIRE(actual_response == expected_response);
            auto count = db.collection("Collection0").count_documents(builder.view());
            REQUIRE(count == 10000);

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
