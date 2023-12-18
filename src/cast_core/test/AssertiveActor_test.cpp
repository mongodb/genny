// Copyright 2022-present MongoDB Inc.
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

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <cast_core/actors/AssertiveActor.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_builder = bsoncxx::builder::basic;

class AssertiveActorTestFixture : public MongoTestFixture {
public:
    void prepareColl(
        const std::string& collName,
        const std::vector<bsoncxx::document::view>& docs
    ) {
        if (!docs.empty()) {
            auto db = client.database("test");
            auto testColl = db.collection(collName);
            testColl.insert_many(docs.begin(), docs.end());
        }
    }

    void prepareDatabase(
        const std::vector<bsoncxx::document::view>& expectedCollDocs = {},
        const std::vector<bsoncxx::document::view>& actualCollDocs = {}
    ) {
        dropAllDatabases();
        prepareColl("expected", expectedCollDocs);
        prepareColl("actual", actualCollDocs);
    }

    /**
     * Creates a YAML config for an AssertiveActor that compares the results of aggregation pipelines
     * against collections 'expected' and 'actual' using the specified value of 'ignoreFields'. If this
     * is an empty string, the 'IgnoreFields' key is omitted from the YAMl config.
     */
    static genny::ActorHelper setupAssertActor(
        const std::string& actorName,
        const std::string& ignoreFields
    ) {
        const auto ignoreFieldsStr = ignoreFields.empty() ? "" : "IgnoreFields: " + ignoreFields;
        const auto yaml = R"(
            SchemaVersion: 2018-07-01
            Clients:
              Default:
                URI: )" + MongoTestFixture::connectionUri().to_string() + R"(
            Actors:
            - Name: )" + actorName + R"(
              Type: AssertiveActor
              Database: test
              Phases:
              - Repeat: 1
                Expected:
                  aggregate: expected
                  pipeline: []
                  cursor: {batchSize: 101}
                Actual:
                  aggregate: actual
                  pipeline: []
                  cursor: {batchSize: 101}
                )" + ignoreFieldsStr;
        NodeSource nodes = NodeSource(yaml, __FILE__);
        return genny::ActorHelper(nodes.root(), 1);
    }

    static void expectAssertPasses(
        const std::string& actorName,
        const std::string& ignoreFields = ""
    ) {
        try {
            genny::ActorHelper ah = setupAssertActor(actorName, ignoreFields);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            INFO("Assert passed");
        } catch (const genny::actor::FailedAssertionException& e) {
            // Did not expect assert to reach here.
            auto diagInfo = boost::diagnostic_information(e);
            FAIL("Assert failed" << diagInfo);
        }
    }

    static void expectAssertFails(
        const std::string& actorName,
        const std::string& ignoreFields = ""
    ) {
        try {
            genny::ActorHelper ah = setupAssertActor(actorName, ignoreFields);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
            FAIL("Assert passed");
        } catch (const genny::actor::FailedAssertionException& e) {
            // Expected assert to reach here.
            INFO("Assert failed");
        }
    }
};

TEST_CASE_METHOD(AssertiveActorTestFixture, "AssertiveActor passes an assert", "[single_node_replset][three_node_replset][sharded][AssertiveActor]") {
    // The test collections are empty, so this should trivially pass.
    prepareDatabase();
    SECTION("Assert passes because empty collections are equivalent") {
        AssertiveActorTestFixture::expectAssertPasses("EmptyCollections");
    }

    // Compare two identical collections containing the following documents:
    // {a: 1, b: 'foo', c: {d: 1}, d: [1, 2, 3]}, {a: 1, e: 1.4}
    auto doc1 = bson_builder::make_document(
        bson_builder::kvp("a", 1),
        bson_builder::kvp("b", "foo"),
        bson_builder::kvp("c", bson_builder::make_document(bson_builder::kvp("d", 1))),
        bson_builder::kvp("d", bson_builder::make_array(1, 2, 3))
    );
    auto doc2 = bson_builder::make_document(
        bson_builder::kvp("a", 1),
        bson_builder::kvp("e", 1.4)
    );
    std::vector<bsoncxx::document::view> docs {doc1, doc2};
    prepareDatabase(docs, docs);
    SECTION("Assert passes because collections are equivalent") {
        AssertiveActorTestFixture::expectAssertPasses("EqualCollections");
    }
}

TEST_CASE_METHOD(AssertiveActorTestFixture, "AssertiveActor fails an assert", "[single_node_replset][three_node_replset][sharded][AssertiveActor]") {
    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a", 1))},
        {bson_builder::make_document(bson_builder::kvp("a", 2))}
    );
    SECTION("Assert fails because {a: 1} differs from {a: 2}") {
        AssertiveActorTestFixture::expectAssertFails("MismatchedInt");
    }

    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a", "foo"))},
        {bson_builder::make_document(bson_builder::kvp("a", "bar"))}
    );
    SECTION("Assert fails because {a: 'foo'} differs from {a: 'bar'}") {
        AssertiveActorTestFixture::expectAssertFails("MismatchedStr");
    }

    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a", 1.0))},
        {bson_builder::make_document(bson_builder::kvp("a", 1.1))}
    );
    SECTION("Assert fails because {a: 1.0} differs from {a: 1.1}") {
        AssertiveActorTestFixture::expectAssertFails("MismatchedDouble");
    }

    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a",
            bson_builder::make_document(bson_builder::kvp("a", 1))))},
        {bson_builder::make_document(bson_builder::kvp("a", 1))}
    );
    SECTION("Assert fails because {a: {a: 1}} differs from {a: 1}") {
        AssertiveActorTestFixture::expectAssertFails("MismatchedDocNested");
    }

    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a", bson_builder::make_array(
            bson_builder::make_document(bson_builder::kvp("a", 1)),
            bson_builder::make_document(bson_builder::kvp("b", 1)))))},
        {bson_builder::make_document(bson_builder::kvp("a", bson_builder::make_array(
            bson_builder::make_document(bson_builder::kvp("a", 1)),
            bson_builder::make_document(bson_builder::kvp("b", 2)))))}
    );
    SECTION("Assert fails because {a: [{a: 1}, {b: 1}]} differs from {a: [{a: 1}, {b: 2}]}") {
        AssertiveActorTestFixture::expectAssertFails("MismatchedArrayNested");
    }
}

TEST_CASE_METHOD(AssertiveActorTestFixture, "AssertiveActor correctly uses IgnoreFields", "[single_node_replset][three_node_replset][sharded][AssertiveActor]") {
    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("a", 1))},
        {bson_builder::make_document(bson_builder::kvp("a", 1))}
    );

    SECTION("Assert fails because '_id' fields differ and 'IgnoreFields' is empty") {
        AssertiveActorTestFixture::expectAssertFails("EmptyIgnoreFieldsActor", "[]");
    }

    SECTION("Assert passes because '_id' fields are ignored by default") {
        AssertiveActorTestFixture::expectAssertPasses("MissingIgnoreFieldsActor");
    }

    prepareDatabase(
        {bson_builder::make_document(bson_builder::kvp("ignoreMe", 1))},
        {bson_builder::make_document(bson_builder::kvp("ignoreMe", 2))}
    );

    SECTION("Assert passes because fields 'ignoreMe' and '_id' are explicitly ignored") {
        AssertiveActorTestFixture::expectAssertPasses("ExplicitlyIgnoreFieldsActor", "['ignoreMe', '_id']");
    }
}

}  // namespace
}  // namespace genny
