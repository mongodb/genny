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

TEST_CASE_METHOD(
    MongoTestFixture,
    "SamplingLoader - demo",
    "[standalone][single_node_replset][three_node_replset][sharded][SamplingLoader]") {

    dropAllDatabases();
    auto db = client.database("test");
    auto collection = db.collection("sampling_loader_test");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() +
                                      R"(
        Actors:
        - Name: CollectionSeeder
          Type: MonotonicSingleLoader
          Threads: 1
          Phases:
          - Repeat: 1
            Database: test
            Collection: sampling_loader_test
            DocumentCount: 5
            BatchSize: 5
            Document: 
              x: {^Inc: {}}
          - {Nop: true}

        # In order to test something this random, we'll use a sample size equal to the collection
        # size, and that way we can verify that every document gets re-inserted the same number of
        # times.
        - Name: SamplingLoader
          Type: SamplingLoader
          Threads: 2
          Phases:
          - {Nop: true}
          - Repeat: 1
            Database: test
            Collection: sampling_loader_test
            # Should see each document inserted an _additional_ 4 times (2 threads, twice each), so
            # should appear 5 times each when complete.
            SampleSize: 5
            InsertBatchSize: 2
            Batches: 5

        Metrics:
          Format: csv

    )",
                                  __FILE__);


    SECTION(
        "Inserts documents, samples all of them and re-inserts, check if documents are duplciated "
        "the right number of times") {
        try {
            genny::ActorHelper ah(nodes.root(), 3 /* 1 thread for loading, 2 for samplers */);
            ah.run();

            // Assert we see each value of "x" occur 5 times, and that there are 5 unique values.
            mongocxx::pipeline pipe;
            pipe.sort_by_count("$x");
            auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
            size_t nResults = 0;
            for (auto&& result : cursor) {
                nResults++;
                REQUIRE(result["count"].get_int32() == 5);
            }
            REQUIRE(nResults == 5);

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
