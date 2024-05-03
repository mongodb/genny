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

TEST_CASE_METHOD(MongoTestFixture, "StreamStatsReporter successfully connects to a MongoDB instance.",
          "[streams][StreamStatsReporter]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
        Actors:
        - Name: StreamStatsReporter
          Type: StreamStatsReporter
          Database: mydb
          Phases:
          - Repeat: 1
            StreamProcessorName: sp
            StreamProcessorId: spid
            ExpectedDocumentCount: 1
    )", __FILE__);


    SECTION("Runs successfully") {
        try {
            auto startCommand = bsoncxx::builder::stream::document{}
                << "streams_startStreamProcessor" << ""
                << "tenantId" << "test"
                << "name" << "sp"
                << "processorId" << "test_spid"
                << "pipeline" << bsoncxx::builder::stream::open_array
                << bsoncxx::builder::stream::open_document
                << "$source" << bsoncxx::builder::stream::open_document << "connectionName" << "__testMemory" << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::open_document
                << "$emit" << bsoncxx::builder::stream::open_document << "connectionName" << "__testMemory" << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_array
                << "connections" << bsoncxx::builder::stream::open_array
                << bsoncxx::builder::stream::open_document
                << "name" << "__testMemory"
                << "type" << "in_memory"
                << "options" << bsoncxx::builder::stream::open_document << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_array
                << "options" << bsoncxx::builder::stream::open_document
                << "featureFlags" << bsoncxx::builder::stream::open_document << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::finalize;
            db.run_command(startCommand.view());

            // Just run to make sure that we don't crash.
            genny::ActorHelper ah(nodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
