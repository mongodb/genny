#include "test.h"

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <ActorHelper.hpp>
#include <MongoTestFixture.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace BasicBson = bsoncxx::builder::basic;
namespace bson_stream = bsoncxx::builder::stream;

class SessionTest {
public:
    SessionTest() {
        mongocxx::options::apm apmOpts;

        apmOpts.on_command_started([&](const mongocxx::events::command_started_event& event) {
            std::string command_name{event.command_name().data()};

            // Ignore auth commands like "saslStart", and handshakes with "isMaster".
            std::string sasl{"sasl"};
            if (event.command_name().substr(0, sasl.size()).compare(sasl) == 0 ||
                command_name.compare("isMaster") == 0) {
                return;
            }

            events.emplace_back(command_name, bsoncxx::document::value(event.command()));
        });
        clientOpts.apm_opts(apmOpts);
    }

    void clearEvents() {
        events.clear();
    }

    class ApmEvent {
    public:
        ApmEvent(const std::string& command_name_, const bsoncxx::document::value& document_)
            : command_name(command_name_), value(document_), command(value.view()) {}

        std::string command_name;
        bsoncxx::document::value value;
        bsoncxx::document::view command;
    };

    std::vector<ApmEvent> events;
    mongocxx::options::client clientOpts;
};


TEST_CASE_METHOD(MongoTestFixture,
                 "CrudActor successfully connects to a MongoDB instance.",
                 "[standalone][single_node_replset][three_node_replset][sharded][CrudActor]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    YAML::Node config = YAML::Load(R"(
      SchemaVersion: 2018-07-01
      Actors:
      - Name: CrudActor
        Type: CrudActor
        Database: mydb
        ExecutionStrategy:
          ThrowOnFailure: true
        Phases:
        - Repeat: 1
          Collection: test
          Operations:
          - OperationName: bulkWrite
            OperationCommand:
              WriteOperations:
              - WriteCommand: insertOne
                Document: { a: 1 }
      )");

    SECTION("Inserts documents into the database.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto builder = bson_stream::document{};
            builder << "a"
                    << "1" << bson_stream::finalize;
            auto count = db.collection("test").count_documents(builder.view());
            REQUIRE(count == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Test bulkWrite operation.",
                 "[standalone][single_node_replset][three_node_replset][sharded][CrudActor]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    SECTION("Inserts and updates document in the database.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 5)));
            REQUIRE(count == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Inserts and deletes documents in the database.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: deleteOne
                    Filter: { a: 1 }
                  - WriteCommand: insertOne
                    Document: { a: 2 }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            REQUIRE(count == 0);
            count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 2)));
            REQUIRE(count == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Inserts and replaces document in the database.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: replaceOne
                    Filter: { a : 1 }
                    Replacement: { name: test }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("name", "test")));
            REQUIRE(count == 1);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Inserts and updates with 'BypassDocumentValidation' true and 'Ordered' false.") {
        SessionTest test;

        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
                  Options:
                    Ordered: false
                    BypassDocumentValidation: true
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 5)));
            REQUIRE(count == 1);
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["ordered"]);
                auto isOrdered = event.command["ordered"].get_bool().value;
                REQUIRE(isOrdered == false);
                REQUIRE(event.command["bypassDocumentValidation"]);
                auto bypassDocValidation =
                    event.command["bypassDocumentValidation"].get_bool().value;
                REQUIRE(bypassDocValidation);
            }
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Inserts and updates many documents in the database.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: updateMany
                    Filter: { a: {$gte: 5} }
                    Update: { $set: { a: 1 } }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            REQUIRE(count == 3);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Inserts and deletes many documents in the database.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 5, max: 15} } }
                  - WriteCommand: deleteMany
                    Filter: { a: {$gte: 5} }
          )");
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(BasicBson::make_document(
                BasicBson::kvp("a", BasicBson::make_document(BasicBson::kvp("$gte", 5)))));
            REQUIRE(count == 0);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Insert randomly generated doc and update with write concern majority.") {
        SessionTest test;
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: {^RandomInt: {min: 2, max: 5} }}
                  - WriteCommand: updateOne
                    Filter: { a: { $gte: 2 } }
                    Update: { $set: { a: 8 } }
                  Options:
                    WriteConcern:
                      Level: majority
                      TimeoutMillis: 6000
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 8)));
            REQUIRE(count == 1);
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["writeConcern"]);
                auto writeConcernLevel = event.command["writeConcern"]["w"].get_utf8().value;
                REQUIRE(std::string(writeConcernLevel) == "majority");
                auto timeout = event.command["writeConcern"]["wtimeout"].get_int32().value;
                REQUIRE(timeout == 6000);
            }

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Test write concern options.",
                 "[standalone][single_node_replset][three_node_replset][sharded][CrudActor]") {

    dropAllDatabases();
    SessionTest test;
    auto db = client.database("mydb");
    test.clearEvents();

    SECTION("Write concern majority with timeout.") {
        YAML::Node config = YAML::Load(R"(
            SchemaVersion: 2018-07-01
            Actors:
            - Name: CrudActor
              Type: CrudActor
              Database: mydb
              ExecutionStrategy:
                ThrowOnFailure: true
              Phases:
              - Repeat: 1
                Collection: test
                Operations:
                - OperationName: bulkWrite
                  OperationCommand:
                    WriteOperations:
                    - WriteCommand: insertOne
                      Document: { a: 1 }
                    - WriteCommand: updateOne
                      Filter: { a: 1 }
                      Update: { $set: { a: 5 } }
                    Options:
                      WriteConcern:
                        Level: majority
                        TimeoutMillis: 5000
            )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["writeConcern"]);
                auto writeConcernLevel = event.command["writeConcern"]["w"].get_utf8().value;
                REQUIRE(std::string(writeConcernLevel) == "majority");
                auto timeout = event.command["writeConcern"]["wtimeout"].get_int32().value;
                REQUIRE(timeout == 5000);
            }

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Write concern 1 with timeout and journalling true.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
                  Options:
                    WriteConcern:
                      Level: 1
                      TimeoutMillis: 2500
                      Journal: true
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["writeConcern"]);
                auto writeConcernLevel = event.command["writeConcern"]["w"].get_int32().value;
                REQUIRE(writeConcernLevel == 1);
                auto timeout = event.command["writeConcern"]["wtimeout"].get_int32().value;
                REQUIRE(timeout == 2500);
                auto journal = event.command["writeConcern"]["j"].get_bool().value;
                REQUIRE(journal);
            }

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Write concern 0 with timeout and journalling false.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
                  Options:
                    Ordered: true
                    WriteConcern:
                      Level: 0
                      TimeoutMillis: 3000
                      Journal: false
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["writeConcern"]);
                auto writeConcernLevel = event.command["writeConcern"]["w"].get_int32().value;
                REQUIRE(writeConcernLevel == 0);
                auto timeout = event.command["writeConcern"]["wtimeout"].get_int32().value;
                REQUIRE(timeout == 3000);
                auto journal = event.command["writeConcern"]["j"].get_bool().value;
                REQUIRE(journal == false);
            }

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Write concern without 'Level' field should throw.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
                  Options:
                    WriteConcern:
                      TimeoutMillis: 3000
                      Journal: false
          )");
        try {
            REQUIRE_THROWS_AS(
                genny::ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                YAML::TypedBadConversion<mongocxx::v_noabi::write_concern>);

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Invalid write concern level should throw.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: bulkWrite
                OperationCommand:
                  WriteOperations:
                  - WriteCommand: insertOne
                    Document: { a: 1 }
                  - WriteCommand: updateOne
                    Filter: { a: 1 }
                    Update: { $set: { a: 5 } }
                  Options:
                    WriteConcern:
                      Level: infinite
          )");
        try {
            REQUIRE_THROWS_AS(
                genny::ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                YAML::TypedBadConversion<mongocxx::v_noabi::write_concern>);

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Test read preference options.",
                 "[standalone][single_node_replset][three_node_replset][sharded][CrudActor]") {

    dropAllDatabases();
    SessionTest test;
    test.clearEvents();
    auto db = client.database("mydb");

    SECTION("Read preference is 'secondaryPreferred'.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: count
                OperationCommand:
                  Filter: { a : 1 }
                  Options:
                    ReadPreference:
                      ReadMode: secondaryPreferred
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["$readPreference"]);
                REQUIRE(event.command["$readPreference"]["mode"]);
                auto readMode = event.command["$readPreference"]["mode"].get_utf8().value;
                REQUIRE(std::string(readMode) == "secondaryPreferred");
            }
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Read preference is 'nearest' with MaxStalenessSeconds set.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: count
                OperationCommand:
                  Filter: { a : 1 }
                  Options:
                    ReadPreference:
                      ReadMode: nearest
                      MaxStalenessSeconds: 100
          )");
        try {
            genny::ActorHelper ah(
                config, 1, MongoTestFixture::connectionUri().to_string(), test.clientOpts);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            REQUIRE(test.events.size() > 0);
            for (auto&& event : test.events) {
                REQUIRE(event.command["$readPreference"]);
                REQUIRE(event.command["$readPreference"]["mode"]);
                auto readMode = event.command["$readPreference"]["mode"].get_utf8().value;
                REQUIRE(std::string(readMode) == "nearest");
                REQUIRE(event.command["$readPreference"]["maxStalenessSeconds"]);
                auto maxStaleness =
                    event.command["$readPreference"]["maxStalenessSeconds"].get_int64().value;
                REQUIRE(maxStaleness == 100);
            }
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Read preference without 'ReadMode' should throw.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: count
                OperationCommand:
                  Filter: { a : 1 }
                  Options:
                    ReadPreference:
                      MaxStalenessSeconds: 100
          )");
        try {
            REQUIRE_THROWS_AS(
                genny::ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                YAML::TypedBadConversion<mongocxx::v_noabi::read_preference>);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    SECTION("Read preference with invalid 'ReadMode' should throw.") {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            ExecutionStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
              Operations:
              - OperationName: count
                OperationCommand:
                  Filter: { a : 1 }
                  Options:
                    ReadPreference:
                      ReadMode: badReadMode
          )");
        try {
            REQUIRE_THROWS_AS(
                genny::ActorHelper(config, 1, MongoTestFixture::connectionUri().to_string()),
                YAML::TypedBadConversion<mongocxx::v_noabi::read_preference>);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

TEST_CASE_METHOD(MongoTestFixture,
                 "Test 'insertMany' operation.",
                 "[standalone][single_node_replset][three_node_replset][sharded][CrudActor]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    YAML::Node config = YAML::Load(R"(
      SchemaVersion: 2018-07-01
      Actors:
      - Name: CrudActor
        Type: CrudActor
        Database: mydb
        ExecutionStrategy:
          ThrowOnFailure: true
        Phases:
        - Repeat: 1
          Collection: test
          Operations:
          - OperationName: insertMany
            OperationCommand:
              Documents:
              - { a: 1 }
              - { a : 1 }
              - { b : 1 }
      )");

    SECTION("The correct documents are inserted.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            auto count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("a", 1)));
            REQUIRE(count == 2);
            count = db.collection("test").count_documents(
                BasicBson::make_document(BasicBson::kvp("b", 1)));
            REQUIRE(count == 1);
            count = db.collection("test").count_documents(BasicBson::make_document());
            REQUIRE(count == 3);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}

// TODO: add test for ReadConcern
// TODO: add test for DropOperation

}  // namespace
