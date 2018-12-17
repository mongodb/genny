#include "test.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <cstdint>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <string_view>
#include <vector>

#include <log.hh>

using namespace std;
namespace bson_stream = bsoncxx::builder::stream;

static const auto client = mongocxx::client(mongocxx::uri{getenv("MONGO_CONNECTION_STRING")});

bsoncxx::stdx::string_view to_stdx_string_view(string s) {
    return bsoncxx::stdx::string_view(s);
}

void teardown() {
    for (auto&& dbDoc : client.list_databases()) {
        const auto dbName = dbDoc["name"].get_utf8().value;
        if (dbName != to_stdx_string_view("admin") && dbName != to_stdx_string_view("config") &&
            dbName != to_stdx_string_view("local")) {
            client.database(dbName).drop();
        }
    }
}

TEST_CASE("Successfully connects to a standalone MongoDB instance.", "[standalone]") {
    auto db = client.database("test");

    SECTION("Insert a document into the database.") {
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder
            << "name"
            << "MongoDB"
            << "type"
            << "database"
            << "count" << 1 << "info" << bson_stream::open_document << "x" << 203 << "y" << 102
            << bson_stream::close_document << bson_stream::finalize;
        bsoncxx::document::view view = doc_value.view();
        db.collection("test").insert_one(view);
        REQUIRE(db.collection("test").count(view) == 1);
    }

    teardown();
}

TEST_CASE("Successfully connects to a single node replica set.", "[single_node_replset]") {
    auto db = client.database("test");

    SECTION("Insert a document into the database.") {
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder
            << "name"
            << "MongoDB"
            << "type"
            << "database"
            << "count" << 1 << "info" << bson_stream::open_document << "x" << 203 << "y" << 102
            << bson_stream::close_document << bson_stream::finalize;
        bsoncxx::document::view view = doc_value.view();
        db.collection("test").insert_one(view);
        REQUIRE(db.collection("test").count(view) == 1);
    }

    teardown();
}

TEST_CASE("Successfully connects to a three node replica set.", "[three_node_replset]") {
    auto db = client.database("test");

    SECTION("Insert a document into the database.") {
        auto builder = bson_stream::document{};
        bsoncxx::document::value doc_value = builder
            << "name"
            << "MongoDB"
            << "type"
            << "database"
            << "count" << 1 << "info" << bson_stream::open_document << "x" << 203 << "y" << 102
            << bson_stream::close_document << bson_stream::finalize;
        bsoncxx::document::view view = doc_value.view();
        db.collection("test").insert_one(view);
        REQUIRE(db.collection("test").count(view) == 1);
    }

    teardown();
}
