#include "test.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include "MongoTestFixture.hpp"

namespace {

using namespace genny::testing;

namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture,
                 "Successfully connects to a MongoDB instance.",
                 "[standalone][single_node_replset][three_node_replset][sharded]") {

    dropAllDatabases();
    auto db = client.database("test");

    auto builder = bson_stream::document{};
    bsoncxx::document::value doc_value = builder
        << "name"
        << "MongoDB"
        << "type"
        << "database"
        << "count" << 1 << "info" << bson_stream::open_document << "x" << 203 << "y" << 102
        << bson_stream::close_document << bson_stream::finalize;

    SECTION("Insert a document into the database.") {

        bsoncxx::document::view view = doc_value.view();
        db.collection("test").insert_one(view);

        REQUIRE(db.collection("test").count(view) == 1);
    }
}
}  // namespace
