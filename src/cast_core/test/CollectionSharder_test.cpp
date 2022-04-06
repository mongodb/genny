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

//
// ⚠️ There is a "known" failure that you should find and fix as a bit of
// an exercise in reading and testing your Actor. ⚠️
//

TEST_CASE_METHOD(MongoTestFixture, "CollectionSharder successfully connects to a MongoDB instance.",
          "[standalone][single_node_replset][three_node_replset][sharded][CollectionSharder]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
        Actors:
        - Name: CollectionSharder
          Type: CollectionSharder
          Phases:
          - ShardCollections:
            - Database: mydb
              Collection: *Collection
              key: {_id: 1}
    )", __FILE__);


    SECTION("Try sharding the database") {
        try {
            genny::ActorHelper ah(nodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            // auto builder = bson_stream::document{};
            // builder << "foo" << bson_stream::open_document
            //         << "$gte" << "0" << bson_stream::close_document
            //         << bson_stream::finalize;

            // auto count = db.collection("mycoll").count_documents(builder.view());
            // // TODO: fixme
            // REQUIRE(count == 101);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
