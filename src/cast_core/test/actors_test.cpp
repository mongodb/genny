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

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

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
