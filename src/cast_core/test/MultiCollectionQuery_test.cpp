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

#include <testlib/helpers.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/json.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>

namespace {
using namespace genny::testing;
namespace BasicBson = bsoncxx::builder::basic;

TEST_CASE_METHOD(MongoTestFixture, "MultiCollectionQuery", "[standalone][MultiCollectionQuery]") {

    dropAllDatabases();
    auto events = ApmEvents{};
    auto db = client.database("mydb");

    SECTION("Query documents in a collection with sort and limit") {
        genny::NodeSource config (R"(
      SchemaVersion: 2018-07-01
      Actors:
      - Name: MultiCollectionQuery
        Type: MultiCollectionQuery
        Threads: 1
        Database: mydb
        CollectionCount: 1
        Filter: {a: 1}
        Limit: 1
        Sort: {a: 1}
        ReadConcern:
          Level: local
        Phases:
        - Repeat: 1
      )", "");

        try {
            auto coll = db.collection("Collection1");
            coll.insert_one(BasicBson::make_document(BasicBson::kvp("a", 1)));
            coll.insert_one(BasicBson::make_document(BasicBson::kvp("a", 2)));

            auto count = coll.count_documents(BasicBson::make_document());
            REQUIRE(count == 2);

            auto apmCallback = makeApmCallback(events);
            genny::ActorHelper ah(
                config.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            for (auto&& event : events) {
                REQUIRE(event.command["limit"].get_int64() == 1);
                REQUIRE(event.command["sort"].get_document().view() ==
                        BasicBson::make_document(BasicBson::kvp("a", 1)));
                auto readMode = event.command["$readPreference"]["mode"].get_utf8().value;
                REQUIRE(std::string(readMode) == "primaryPreferred");
            }

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace