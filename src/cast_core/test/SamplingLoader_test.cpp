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
                 "[standalone][single_node_replset][three_node_replset][sharded][SamplingLoader]") {

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
        # In order to test something this random, we'll use a sample size equal to the collection
        # size, and that way we can verify that every document gets re-inserted the same number of
        # times. A smaller sample size would be non-determinisitc.
        - Name: SamplingLoader
          Type: SamplingLoader
          Threads: 2
          Phases:
          - Repeat: 1
            Database: test
            Collection: sampling_loader_test
            # Should see each document inserted an _additional_ 8 times
            # (2 threads x 2 batches x 2 per batch (batch size = 10)), so should appear 9 times each
            # when complete.
            SampleSize: 5
            InsertBatchSize: 10
            Pipeline: [{$set: {y: "SamplingLoader wuz here"}}]
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

            // Assert we see each value of "x" occur 5 times, and that there are 5 unique values.
            mongocxx::pipeline pipe;
            pipe.sort_by_count("$x");
            auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
            size_t nResults = 0;
            for (auto&& result : cursor) {
                nResults++;
                REQUIRE(result["count"].get_int32() == 9);
            }
            REQUIRE(nResults == 5);

            // Assert that the Pipeline was run and we should see 8*5 = 40 documents with a 'y'
            // field.
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
