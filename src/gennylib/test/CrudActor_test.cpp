#include "test.h"

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <MongoTestFixture.hpp>
#include <ActorHelper.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture, "CrudActor successfully connects to a MongoDB instance.",
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
          - Repeat: 100
            Collection: mycoll
            Document: {foo: {$randomint: {min: 0, max: 100}}}
    )");


    SECTION("Inserts documents into the database.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            auto builder = bson_stream::document{};
            builder << "foo" << bson_stream::open_document
                    << "$gte" << "0" << bson_stream::close_document
                    << bson_stream::finalize;

            auto count = db.collection("mycoll").count_documents(builder.view());
            // TODO: fixme
            REQUIRE(count == 101);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
} // namespace
