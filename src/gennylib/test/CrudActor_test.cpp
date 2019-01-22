#include "test.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <ActorHelper.hpp>
#include <MongoTestFixture.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

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
        Options:
          WriteConcern:
            Level: majority
            TimeoutMillis: 5000
)");

    SECTION("Inserts documents into the database.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
