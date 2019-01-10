#include "ActorHelper.hpp"
#include "test.h"

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include <bsoncxx/json.hpp>

#include <cast_core/actors/RunCommand.hpp>
#include <gennylib/MongoException.hpp>

#include <mongocxx/read_preference.hpp>

#include "MongoTestFixture.hpp"

namespace genny {
namespace {

using namespace genny::testing;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

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

TEST_CASE_METHOD(MongoTestFixture, 
                 "InsertActor respects writeConcern.",
                 "[three_node_replset]") {

    YAML::Node config_w3 = YAML::Load(R"(
        SchemaVersion: 2018-07-01

        Actors:
        - Name: TestInsertWriteConcern
          Type: RunCommand
          Threads: 1
          Phases:
          - Repeat: 1
            Database: mydb
            Operation:
              OperationName: RunCommand
              OperationCommand:
                insert: myCollection
                documents: [{name: myName}]
                writeConcern: {w: 3, wtimeout: 5000}
    )");
    
    YAML::Node config_w1 = YAML::Load(R"(
        SchemaVersion: 2018-07-01
        
        Actors:
        - Name: TestInsertWriteConcern
          Type: RunCommand 
          Threads: 1
          Phases:
          - Repeat: 1
            Database: mydb
            Operation:
              OperationName: RunCommand
              OperationCommand:
                insert: myCollection
                documents: [{name: myOtherName}]
                writeConcern: {w: 1, wtimeout: 5000}
    )");

    SECTION("verify write concern to secondaries") {
        ActorHelper ah(config_w3, 1, MongoTestFixture::connectionUri().to_string());
        ah.run([](const WorkloadContext& wc) { wc.actors()[0]->run(); });
    
        auto session = MongoTestFixture::client.start_session();
        auto coll = MongoTestFixture::client["mydb"]["myCollection"];
        
        mongocxx::options::find opts;        
        mongocxx::read_preference secondary;
        secondary.mode(mongocxx::read_preference::read_mode::k_secondary);
        opts.read_preference(secondary).max_time(std::chrono::milliseconds(2000));
        
        bool result = (bool) coll.find_one(session, make_document(kvp("name", "myName")), opts);
        
        REQUIRE(result);
    }        
    
    SECTION("verify write concern to primary only") {
        ActorHelper ah(config_w1, 1, MongoTestFixture::connectionUri().to_string());
        
        ah.run([](const WorkloadContext& wc) { wc.actors()[0]->run(); });
        
        auto session = MongoTestFixture::client.start_session();
        auto coll = MongoTestFixture::client["mydb"]["myCollection"];
        
        mongocxx::options::find opts;
        
        mongocxx::read_preference secondary;
        secondary.mode(mongocxx::read_preference::read_mode::k_secondary);
        mongocxx::read_preference primary;
        secondary.mode(mongocxx::read_preference::read_mode::k_secondary);
        
        opts.read_preference(secondary).max_time(std::chrono::milliseconds(2000));
        bool result_secondary = (bool) coll.find_one(session, make_document(kvp("name", "myOtherName")), opts);
        REQUIRE(!result_secondary);
        
        opts.read_preference(primary).max_time(std::chrono::milliseconds(2000));
        bool result_primary = (bool) coll.find_one(session, make_document(kvp("name", "myOtherName")), opts);
        REQUIRE(result_primary);
    }
}
}  // namespace
}  // namespace genny
