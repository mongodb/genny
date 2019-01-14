#include "ActorHelper.hpp"
#include "test.h"

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include <bsoncxx/json.hpp>

#include <boost/log/trivial.hpp>

#include <cast_core/actors/RunCommand.hpp>
#include <gennylib/MongoException.hpp>

#include <mongocxx/read_preference.hpp>

#include "MongoTestFixture.hpp"

namespace BasicBson = bsoncxx::builder::basic;

namespace genny {
namespace {

using namespace genny::testing;

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

        BOOST_LOG_TRIVIAL(info) << MongoTestFixture::client.uri().to_string() << std::endl;

        auto yamlConfig = makeConfig();

        [&](YAML::Node yamlPhase) {
            yamlPhase["Database"] = dbStr;
            yamlPhase["Operation"]["OperationCommand"]["insert"] = collectionStr;
            yamlPhase["Operation"]["OperationCommand"]["writeConcern"]["w"] = 3;
        }(yamlConfig["Actors"][0]["Phases"][0]);

        ActorHelper ah(yamlConfig, 1, MongoTestFixture::connectionUri().to_string());
        ah.run();

        auto coll = MongoTestFixture::client["test"]["testCollection"];

        mongocxx::options::find opts = makeFindOp(mongocxx::read_preference::read_mode::k_secondary, readTimeout);

        auto result = static_cast<bool>(coll.find_one(session, BasicBson::make_document(BasicBson::kvp("name", "myName")), opts));

        REQUIRE(result);
    }
    
    
    // TODO: Once better repl support comes in, pause replication for this test case. With replication paused, we want to 
    // test that reads to secondaries will fail on primary write concerns. Without the paused replication, the test currently
    // is a bit flaky. Refer to jstests/libs/write_concern_util.js in the main mongo repo for pausing replication.
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
    //     mongocxx::options::find opts_primary = makeFindOp(mongocxx::read_preference::read_mode::k_primary, readTimeout);
    //     mongocxx::options::find opts_secondary = makeFindOp(mongocxx::read_preference::read_mode::k_secondary, readTimeout);
    // 
    //     bool result_secondary = (bool) coll.find_one(session, BasicBson::make_document(BasicBson::kvp("name", "myName")), opts_secondary);
    //     REQUIRE(!result_secondary);
    // 
    //     bool result_primary = (bool) coll.find_one(session, BasicBson::make_document(BasicBson::kvp("name", "myName")), opts_primary);
    //     REQUIRE(result_primary);
    // }
}
}  // namespace
}  // namespace genny
