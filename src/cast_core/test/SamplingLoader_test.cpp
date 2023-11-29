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

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/context.hpp>
#include <regex>
#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;
using namespace bsoncxx;

TEST_CASE_METHOD(MongoTestFixture,
                 "SamplingLoader - demo",
                 "[single_node_replset][three_node_replset][sharded][SamplingLoader]") {

    dropAllDatabases();
    auto db = client.database("test");
    auto collection = db.collection("sampling_loader_test");
    // Seed some data.
    collection.insert_many(std::vector{from_json("{\"x\": 0}"),
                                       from_json("{\"x\": 1}"),
                                       from_json("{\"x\": 2}"),
                                       from_json("{\"x\": 3}"),
                                       from_json("{\"x\": 4}")});

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() +
                                      R"(
        Actors:

        # Insert 40 new documents (batches=2 * batchSize=10 * threads=2), each with a 'y' field.
        - Name: SamplingLoader
          Type: SamplingLoader
          Database: test
          Collection: sampling_loader_test
          SampleSize: 5
          Pipeline: [{$set: {y: "SamplingLoader wuz here"}}]
          Threads: 2
          Phases:
          - Repeat: 1
            InsertBatchSize: 10
            Batches: 2

        Metrics:
          Format: csv

    )",
                                  __FILE__);


    SECTION(
        "Samples and re-inserts documents, then checks if documents are duplciated the right "
        "number of times") {
        try {
            REQUIRE(collection.count_documents(bsoncxx::document::view()) == 5);
            genny::ActorHelper ah(nodes.root(), 2 /* 2 threads for samplers */);
            ah.run();

            // There should still be only 5 distinct values of 'x'.
            mongocxx::pipeline pipe;
            pipe.group(from_json(R"({"_id": "$x"})"));
            auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
            REQUIRE(std::distance(cursor.begin(), cursor.end()) == 5);

            // There should be 40 new documents, and each new document should have a 'y' field.
            REQUIRE(collection.count_documents(bsoncxx::document::view()) == 45);
            REQUIRE(collection.count_documents(from_json(R"({"y": {"$exists": true}})")) == 40);

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
