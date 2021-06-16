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

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;

TEST_CASE_METHOD(
    MongoTestFixture,
    "MonotonicSingleLoader",
    "[standalone][single_node_replset][three_node_replset][sharded][MonotonicSingleLoader]") {

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: LoadInitialData
          Type: MonotonicSingleLoader
          Threads: 2
          Phases:
          - Repeat: 1
            Database: mydb
            Collection: mycoll
            BatchSize: 1000
            DocumentCount: 10_000
            Document: {field: {^RandomInt: {min: 0, max: 100}}}
    )",
                                  __FILE__);


    SECTION("Inserts documents into the collection") {
        try {
            dropAllDatabases();
            auto db = client.database("mydb");

            genny::ActorHelper ah(nodes.root(), 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            auto filter = bsoncxx::builder::basic::make_document();
            auto count = db.collection("mycoll").count_documents(filter.view());
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
