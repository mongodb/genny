#include "ActorHelper.hpp"
#include "test.h"

#include <boost/exception/diagnostic_information.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <MongoTestFixture.hpp>
#include <cast_core/actors/RunCommand.hpp>

namespace genny {
namespace {

using namespace genny::testing;

TEST_CASE_METHOD(MongoTestFixture,
                 "RunCommandActor successfully connects to a MongoDB instance.",
                 "[standalone][single_node_replset][three_node_replset][sharded]") {

    dropAllDatabases();

    auto db = client.database("test");

    using bsoncxx::builder::basic::document;
    using bsoncxx::builder::basic::kvp;

    auto doc = document{};
    doc.append(kvp("someKey", "someValue"));

    auto rcProducer =
        std::make_shared<DefaultActorProducer<genny::actor::RunCommand>>("RunCommand");

    YAML::Node config = YAML::Load(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: RunCommand
          Type: RunCommand
          Database: mydb
          Phases:
          - Repeat: 1
    )");

    ActorHelper ah(config, 20, {{"RunCommand", rcProducer}});
    ah.run();

    SECTION("throws error with full context on operation_exception") {
        bool has_exception = true;

        try {
            ah.run();
            has_exception = false;
        } catch (boost::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);

            REQUIRE(diagInfo.find("someKey") != std::string::npos);
            REQUIRE(diagInfo.find("command_object") != std::string::npos);

            REQUIRE(diagInfo.find("no such command") != std::string::npos);
            REQUIRE(diagInfo.find("server_response") != std::string::npos);
        }

        // runCommandHelper did not throw exception.
        REQUIRE(has_exception);
    }
}
}  // namespace
}  // namespace genny
