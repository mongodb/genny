#include "ActorHelper.hpp"
#include "test.h"

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <cast_core/actors/RunCommand.hpp>
#include <gennylib/MongoException.hpp>

#include "MongoTestFixture.hpp"

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
}  // namespace
}  // namespace genny
