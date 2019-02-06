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

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/read_preference.hpp>

#include <cast_core/actors/RunCommand.hpp>

#include <gennylib/MongoException.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

namespace genny {
namespace {

using namespace genny::testing;
namespace BasicBson = bsoncxx::builder::basic;
namespace bson_stream = bsoncxx::builder::stream;

const auto dropAdminTestCollConfig = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    Actors:
    - Name: TestActor
      Type: AdminCommand
      Threads: 1
      Phases:
      - Repeat: 1
        Operations:
        - OperationName: AdminCommand
          OperationCommand:
            drop: testCollection
)");

// Don't run in a sharded cluster because the error message is different.
TEST_CASE_METHOD(MongoTestFixture,
                 "RunCommandActor successfully connects to a MongoDB instance.",
                 "[standalone][single_node_replset][three_node_replset]") {

    YAML::Node config = YAML::Load(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: TestRunCommand
          Type: RunCommand
          ExecutionStrategy:
            ThrowOnFailure: true
          Phases:
          - Repeat: 1
            Database: mydb
            Type: RunCommand
            Operation:
              OperationCommand: {someKey: 1}
    )");

    ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());

    SECTION("throws error with full context on operation_exception") {
        bool has_exception = true;

        try {
            ah.run([](const WorkloadContext& wc) { wc.actors()[0]->run(); });
            has_exception = false;
        } catch (const boost::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);

            REQUIRE(diagInfo.find("someKey") != std::string::npos);
            REQUIRE(diagInfo.find("InfoObject") != std::string::npos);

            REQUIRE(diagInfo.find("no such command") != std::string::npos);
            REQUIRE(diagInfo.find("ServerResponse") != std::string::npos);
        }

        // runCommandHelper did not throw exception.
        REQUIRE(has_exception);
    }
}

TEST_CASE_METHOD(MongoTestFixture, "InsertActor respects writeConcern.", "[three_node_replset]") {
    auto events = ApmEvents{};

    auto makeConfig = []() {
        return YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestInsertWriteConcern
              Type: RunCommand
              Threads: 1
              Phases:
              - Repeat: 1
                Operation:
                    OperationName: RunCommand
                    OperationCommand:
                        insert:
                        documents: [{name: myName}]
                        writeConcern: {wtimeout: 5000}

        )");
    };

    auto makeFindOp = [](mongocxx::read_preference::read_mode readPreference, int timeout) {
        mongocxx::options::find opts;
        mongocxx::read_preference preference;

        preference.mode(readPreference);
        opts.read_preference(preference).max_time(std::chrono::milliseconds(timeout));

        return opts;
    };

    constexpr auto readTimeout = 6000;

    SECTION("verify write concern to secondaries") {
        constexpr auto dbStr = "test";
        constexpr auto collectionStr = "testCollection";

        auto session = MongoTestFixture::client.start_session();

        auto yamlConfig = makeConfig();

        [&](YAML::Node yamlPhase) {
            yamlPhase["Database"] = dbStr;
            yamlPhase["Operation"]["OperationCommand"]["insert"] = collectionStr;
            yamlPhase["Operation"]["OperationCommand"]["writeConcern"]["w"] = 3;
        }(yamlConfig["Actors"][0]["Phases"][0]);

        auto apmCallback = makeApmCallback(events);
        ActorHelper ah(yamlConfig, 1, MongoTestFixture::connectionUri().to_string(), apmCallback);
        ah.run();
        auto coll = MongoTestFixture::client["test"]["testCollection"];
        REQUIRE(events.size() > 0);
        auto&& event = events.front();
        REQUIRE(event.command["writeConcern"]);
        auto writeConcernLevel = event.command["writeConcern"]["w"].get_int32().value;
        REQUIRE(writeConcernLevel == 3);
    }


    // TODO: Once better repl support comes in, pause replication for this test case. With
    // replication paused, we want to test that reads to secondaries will fail on primary write
    // concerns. Without the paused replication, the test currently is a bit flaky. Refer to
    // jstests/libs/write_concern_util.js in the main mongo repo for pausing replication.
    //
    // SECTION("verify write concern to primary only") {
    //     constexpr auto dbStr = "test";
    //     constexpr auto collectionStr = "testOtherCollection";
    //
    //     auto yamlConfig = makeConfig();
    //
    //     [&](YAML::Node yamlPhase) {
    //         yamlPhase["Database"] = dbStr;
    //         yamlPhase["Operation"]["OperationCommand"]["insert"] = collectionStr;
    //         yamlPhase["Operation"]["OperationCommand"]["writeConcern"]["w"] = 1;
    //     }(yamlConfig["Actors"][0]["Phases"][0]);
    //
    //     ActorHelper ah(yamlConfig, 1, MongoTestFixture::connectionUri().to_string());
    //
    //     ah.run([](const WorkloadContext& wc) { wc.actors()[0]->run(); });
    //
    //     auto session = MongoTestFixture::client.start_session();
    //     auto coll = MongoTestFixture::client["test"]["testOtherCollection"];
    //
    //     mongocxx::options::find opts_primary =
    //     makeFindOp(mongocxx::read_preference::read_mode::k_primary, readTimeout);
    //     mongocxx::options::find opts_secondary =
    //     makeFindOp(mongocxx::read_preference::read_mode::k_secondary, readTimeout);
    //
    //     bool result_secondary = (bool) coll.find_one(session,
    //     BasicBson::make_document(BasicBson::kvp("name", "myName")), opts_secondary);
    //     REQUIRE(!result_secondary);
    //
    //     bool result_primary = (bool) coll.find_one(session,
    //     BasicBson::make_document(BasicBson::kvp("name", "myName")), opts_primary);
    //     REQUIRE(result_primary);
    // }
}

// Don't run in a sharded cluster to avoid 'CannotImplicitlyCreateCollection' exceptions. These do
// not test any sharding-specific behavior.
TEST_CASE_METHOD(MongoTestFixture,
                 "Perform a single RunCommand command.",
                 "[standalone][single_node_replset][three_node_replset]") {
    dropAllDatabases();
    auto db = client.database("test");

    SECTION("Insert a single document using the 'Operations' key name.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
        )");
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder << "rating" << 10 << bson_stream::finalize;
        auto view = doc_value.view();
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, view](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.collection("testCollection").count(view) == 1);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("'Operations' of non-sequence type should throw.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations: 5
        )");
        REQUIRE_THROWS_AS(ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                          InvalidConfigurationException);
    }

    SECTION("Insert a single document using the 'Operation' key name.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
        )");
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder << "rating" << 10 << bson_stream::finalize;
        auto view = doc_value.view();
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, view](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.collection("testCollection").count(view) == 1);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Insert a single document with a field defined by the value generator.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: RunCommand
                  OperationCommand:
                    findAndModify: testCollection
                    query: {rating: {^RandomInt: {min: 1, max: 4}}}
                    update: {$set: {rating: {^RandomInt: {min: 5, max: 10}}}}
                    upsert: true
        )");
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder << "rating" << bson_stream::open_document
                                                     << "$gte" << 5 << bson_stream::close_document
                                                     << bson_stream::finalize;
        auto view = doc_value.view();
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, view](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.collection("testCollection").count(view) == 1);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Having neither 'Operation' nor 'Operations' should throw.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
            Type: RunCommand
            Threads: 1
            Database: test
            Phases:
            - Repeat: 1
              OperationName: RunCommand
              OperationCommand:
                insert: testCollection
                documents: [{rating: 10}]
              OperationName: RunCommand
              OperationCommand:
                insert: testCollection
                documents: [{rating: 10}]
        )");
        REQUIRE_THROWS_AS(ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                          InvalidConfigurationException);
    }

    SECTION("Database should default to 'admin' when not specified in the the config.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
        )");
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder << "rating" << 10 << bson_stream::finalize;
        auto view = doc_value.view();
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto adminDb = client.database("admin");
        auto verifyFn = [&adminDb, view](const WorkloadContext& context) {
            REQUIRE(adminDb.has_collection("testCollection"));
            REQUIRE(adminDb.collection("testCollection").count(view) == 1);
        };

        ah.runDefaultAndVerify(verifyFn);

        // Clean up the newly created collection in the 'admin' database.
        ActorHelper dropCollActor(
            dropAdminTestCollConfig, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyDropFn = [&adminDb](const WorkloadContext& context) {
            REQUIRE_FALSE(adminDb.has_collection("testCollection"));
        };
        dropCollActor.runDefaultAndVerify(verifyDropFn);
    }
}

// Don't run in a sharded cluster to avoid 'CannotImplicitlyCreateCollection' exceptions. These do
// not test any sharding-specific behavior.
TEST_CASE_METHOD(MongoTestFixture,
                 "AdminCommand actor with a single operation.",
                 "[standalone][single_node_replset][three_node_replset]") {
    dropAllDatabases();
    auto adminDb = client.database("admin");

    SECTION("Create a collection in the 'admin' database.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: AdminCommand
              Threads: 1
              Database: admin
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection
        )");
        REQUIRE_FALSE(adminDb.has_collection("testCollection"));
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyCreateFun = [&adminDb](const WorkloadContext& context) {
            REQUIRE(adminDb.has_collection("testCollection"));
        };
        ah.runDefaultAndVerify(verifyCreateFun);

        // Clean up the newly created collection in the 'admin' database.
        ActorHelper dropCollActor(
            dropAdminTestCollConfig, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyDropFn = [&adminDb](const WorkloadContext& context) {
            REQUIRE_FALSE(adminDb.has_collection("testCollection"));
        };
        dropCollActor.runDefaultAndVerify(verifyDropFn);
    }

    SECTION("Database should default to 'admin' when not specified in the the config.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: AdminCommand
              Threads: 1
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection
        )");
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyCreateFn = [&adminDb](const WorkloadContext& context) {
            REQUIRE(adminDb.has_collection("testCollection"));
        };
        ah.runDefaultAndVerify(verifyCreateFn);

        // Clean up the newly created collection in the 'admin' database.
        ActorHelper dropCollActor(
            dropAdminTestCollConfig, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyDropFn = [&adminDb](const WorkloadContext& context) {
            REQUIRE_FALSE(adminDb.has_collection("testCollection"));
        };
        dropCollActor.runDefaultAndVerify(verifyDropFn);
    }
    SECTION("Running an AdminCommand on a non-admin database should throw.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: AdminCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection
        )");
        REQUIRE_THROWS_AS(ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                          InvalidConfigurationException);
    }
}

// Don't run in a sharded cluster to avoid 'CannotImplicitlyCreateCollection' exceptions. These do
// not test any sharding-specific behavior.
TEST_CASE_METHOD(MongoTestFixture,
                 "Performing multiple operations.",
                 "[standalone][single_node_replset][three_node_replset]") {
    dropAllDatabases();
    auto adminDb = client.database("admin");
    auto db = client.database("test");

    SECTION("Create a collection and then insert a document.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents:
                    - {rating: {^RandomInt: {min: 10, max: 10}}, name: y}
                    - {rating: 10, name: x}
        )");
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder << "rating" << 10 << bson_stream::finalize;
        auto view = doc_value.view();
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, adminDb, view](const WorkloadContext& context) {
            REQUIRE_FALSE(adminDb.has_collection("testCollection"));
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.collection("testCollection").count(view) == 2);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Database should default to 'admin' if not specified in the config.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection
                - OperationName: AdminCommand
                  OperationCommand:
                    drop: testCollection
        )");
        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&adminDb](const WorkloadContext& context) {
            REQUIRE_FALSE(adminDb.has_collection("testCollection"));
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Having both 'Operation' and 'Operations' should throw.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operation:
                  OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 15}]
        )");
        REQUIRE_THROWS_AS(ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                          InvalidConfigurationException);
    }
}

// Don't run in a sharded cluster to avoid 'CannotImplicitlyCreateCollection' exceptions. These do
// not test any sharding-specific behavior.
TEST_CASE_METHOD(MongoTestFixture,
                 "Test metric reporting.",
                 "[standalone][single_node_replset][three_node_replset]") {
    dropAllDatabases();
    auto db = client.database("test");

    SECTION("Insert a single document with operation metrics reported.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
                  OperationMetricsName: InsertMetric
        )");

        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, &ah](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));

            std::string_view metricsOutput = ah.getMetricsOutput();
            auto metricNamePos = metricsOutput.rfind("InsertMetric");

            // Naive check that the metrics output contains the substring equal to the metric name.
            REQUIRE(metricNamePos != std::string_view::npos);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Insert a single document with operation metrics not reported.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
        )");

        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
        };
        ah.runDefaultAndVerify(verifyFn);

        std::string_view metricsOutput = ah.getMetricsOutput();
        auto metricNamePos = metricsOutput.rfind("InsertMetric");

        // Naive check that the metrics output doesn't contain the substring equal to the metric
        // name.
        REQUIRE(metricNamePos == std::string_view::npos);
    }

    SECTION("Have multiple operation metrics reported.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
                  OperationMetricsName: InsertMetric
                - OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection2
                  OperationMetricsName: CreateCollectionMetric
        )");

        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, &ah](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.has_collection("testCollection2"));

            std::string_view metricsOutput = ah.getMetricsOutput();
            auto insertMetricNamePos = metricsOutput.rfind("InsertMetric");
            auto createCollMetricNamePos = metricsOutput.rfind("CreateCollectionMetric");

            // Naive check that the metrics output contains the substring equal to the metric name.
            REQUIRE(insertMetricNamePos != std::string_view::npos);
            REQUIRE(createCollMetricNamePos != std::string_view::npos);
        };
        ah.runDefaultAndVerify(verifyFn);
    }

    SECTION("Metrics reported for only one of the two operations listed.") {
        auto config = YAML::Load(R"(
            SchemaVersion: 2018-07-01

            Actors:
            - Name: TestActor
              Type: RunCommand
              Threads: 1
              Database: test
              Phases:
              - Repeat: 1
                Operations:
                - OperationName: RunCommand
                  OperationCommand:
                    insert: testCollection
                    documents: [{rating: 10}]
                - OperationName: AdminCommand
                  OperationCommand:
                    create: testCollection2
                  OperationMetricsName: CreateCollectionMetric
        )");

        ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
        auto verifyFn = [&db, &ah](const WorkloadContext& context) {
            REQUIRE(db.has_collection("testCollection"));
            REQUIRE(db.has_collection("testCollection2"));

            std::string_view metricsOutput = ah.getMetricsOutput();
            auto insertMetricNamePos = metricsOutput.rfind("InsertMetric");
            auto createCollMetricNamePos = metricsOutput.rfind("CreateCollectionMetric");

            // Naive check that the metrics output contains the substring equal to the metric name.
            REQUIRE(insertMetricNamePos == std::string_view::npos);
            REQUIRE(createCollMetricNamePos != std::string_view::npos);
        };
        ah.runDefaultAndVerify(verifyFn);
    }
}
}  // namespace
}  // namespace genny
