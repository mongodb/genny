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

void populate(mongocxx::client& client) {
    auto db0 = client.database("db0");
    auto db1 = client.database("db1");
    auto coll00 = db0.collection("Collection0-db0");
    auto coll01 = db0.collection("Collection1-db0");
    auto coll10 = db1.collection("Collection0-db1");
    auto coll11 = db1.collection("Collection1-db1");
    auto coll12 = db1.collection("Collection2-db1");
    for (int i = 0; i < 100; i++) {
        coll00.insert_one(BasicBson::make_document(BasicBson::kvp("a", i)));
        coll01.insert_one(BasicBson::make_document(BasicBson::kvp("a", i)));
        coll10.insert_one(BasicBson::make_document(BasicBson::kvp("a", i)));
        coll11.insert_one(BasicBson::make_document(BasicBson::kvp("a", i)));
        coll12.insert_one(BasicBson::make_document(BasicBson::kvp("a", i)));
    }
    auto count = coll00.count_documents(BasicBson::make_document());
    REQUIRE(count == 100);
    count = coll01.count_documents(BasicBson::make_document());
    REQUIRE(count == 100);
    count = coll10.count_documents(BasicBson::make_document());
    REQUIRE(count == 100);
    count = coll11.count_documents(BasicBson::make_document());
    REQUIRE(count == 100);
    count = coll12.count_documents(BasicBson::make_document());
    REQUIRE(count == 100);
}

void testOneActor(genny::NodeSource& config,
                  uint32_t requiredSeconds,
                  const std::string& requiredEventStrings) {
    auto events = ApmEvents{};
    auto apmCallback = makeApmCallback(events);
    genny::ActorHelper ah(
        config.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);
    auto start = std::chrono::steady_clock::now();
    ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);

    // Make sure that we ran for the required time period, and
    // got expected events.
    REQUIRE(seconds.count() >= requiredSeconds);
    std::string eventStrings;
    for (auto&& event : events) {
        eventStrings += event.command_name + ":";
    }
    REQUIRE(eventStrings == requiredEventStrings);
}


TEST_CASE_METHOD(MongoTestFixture, "CollectionScanner", "[standalone][CollectionScanner]") {

    SECTION("Scan documents in all collections of a database") {
        genny::NodeSource config(R"(
      SchemaVersion: 2018-07-01
      Actors:
      - Name: SnapshotScanner
        Type: CollectionScanner
        Threads: 1
        Database: db0
        Phases:
        - Duration: 5 seconds
          ScanType: snapshot
          CollectionSortOrder: forward
          GlobalRate: 1 per 3 seconds
        - Duration: 5 seconds
          ScanType: snapshot
          GlobalRate: 1 per 3 seconds
          Documents: 20
      Metrics:
        Format: csv
      )",
                                 "");

        try {
            dropAllDatabases();
            populate(client);

            // In each phase, there is time to call the actor twice.
            // In the first phase, expected events for each invocation of
            // the actor are:
            //  listCollections  (for db0, yielding two collections).
            //  find (for first collection)
            //  find (for second collection).
            // In the second phase, we limited to 20 documents, so we
            // only do a find on the first collection.
            testOneActor(config,
                         10,
                         "listCollections:find:find:"  // 1st phase, 1st call
                         "listCollections:find:find:"  // 1st phase, 2nd call
                         "listCollections:find:"       // 2nd phase, 1st call
                         "listCollections:find:");     // 2nd phase, 2nd call
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture, "CollectionScannerAll", "[standalone][CollectionScanner]") {

    SECTION("Scan documents in all collections of multiple databases") {
        genny::NodeSource config2(R"(
      SchemaVersion: 2018-07-01
      Actors:
      - Name: SnapshotScannerAll
        Type: CollectionScanner
        Threads: 1
        Database: db0,db1
        Phases:
        - Duration: 5 seconds
          ScanType: snapshot
          CollectionSortOrder: forward
          GlobalRate: 1 per 3 seconds
        - Duration: 5 seconds
          ScanType: snapshot
          CollectionSortOrder: forward
          GlobalRate: 1 per 3 seconds
          Documents: 350
      Metrics:
        Format: csv
      )",
                                  "");

        try {
            dropAllDatabases();
            populate(client);

            // In the first phase, there is time to call the actor twice.
            // Expected events for each invocation of the actor are:
            //  listCollections  (for db0, yielding two collections).
            //  listCollections  (for db1, yielding two collections).
            //  2 finds for db0's collections, 3 finds for db1's collections.
            // In the second phase, we are limiting to 350 documents, so
            // we'll satisfy that in the middle of the 4th collection scan.
            testOneActor(config2,
                         5,
                         "listCollections:listCollections:"  // 1st phase, 1st call
                         "find:find:find:find:find:"
                         "listCollections:listCollections:"  // 1st phase, 2nd call
                         "find:find:find:find:find:"
                         "listCollections:listCollections:"  // 2nd phase, 1st call
                         "find:find:find:find:"
                         "listCollections:listCollections:"  // 2nd phase, 2nd call
                         "find:find:find:find:");

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
