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

#include <vector>

#include <boost/algorithm/string/trim.hpp>
#include <boost/exception/detail/exception_ptr.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlTest.hpp>
#include <testlib/yamlToBson.hpp>

namespace genny::testing {

namespace {

const char* DEFAULT_DB = "mydb";
const char* DEFAULT_COLLECTION = "test";

enum class RunMode { kNormal, kExpectedSetupException, kExpectedRuntimeException };

RunMode convertRunMode(YAML::Node tcase) {
    if (tcase["OutcomeData"] || tcase["OutcomeCounts"] || tcase["ExpectAllEvents"] ||
        tcase["ExpectedCollectionsExist"]) {
        return RunMode::kNormal;
    }
    if (tcase["Error"]) {
        auto error = tcase["Error"].as<std::string>();
        if (error == "InvalidSyntax") {
            return RunMode::kExpectedSetupException;
        }
        return RunMode::kExpectedRuntimeException;
    }
    BOOST_THROW_EXCEPTION(std::logic_error("Invalid test-case"));
}


void requireExpectedCollectionsExist(mongocxx::pool::entry& client,
                                     ApmEvents& events,
                                     YAML::Node expectCollections) {
    auto db = (*client)[DEFAULT_DB];
    auto haystack = db.list_collection_names();

    for (auto&& kvp : expectCollections) {
        auto needle = kvp.first.as<std::string>();
        auto expect = kvp.second.as<bool>();
        auto actual = std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
        INFO("Expecting collection " << needle << "to exist? " << actual);
        REQUIRE(expect == actual);
    }
}

bool isNumeric(YAML::Node node) {
    try {
        node.as<long>();
        return true;
    } catch (const std::exception& x) {
    }
    return false;
}

// This isn't ideal. We want to assert only a subset of the keys in the ApmEvents
// that we receive. Doing a subsetCompare(YAML::Node, bsoncxx::document::view) is nontrivial.
// (Mostly fighting the compiler with yaml scalars versus the many numeric bson types.)
// For now just assert a subset
void requireEvent(ApmEvent& event, YAML::Node requirements) {
    if (auto w = requirements["writeConcern"]["w"]; w) {
        if (isNumeric(w)) {
            REQUIRE(event.command["writeConcern"]["w"].get_int32() == w.as<int32_t>());
        } else {
            REQUIRE(event.command["writeConcern"]["w"].get_utf8().value == w.as<std::string>());
        }
    }
    if (auto j = requirements["writeConcern"]["j"]; j) {
        REQUIRE(event.command["writeConcern"]["j"].get_bool().value == j.as<bool>());
    }
    if (auto wtimeout = requirements["writeConcern"]["wtimeout"]; wtimeout) {
        REQUIRE(event.command["writeConcern"]["wtimeout"].get_int32() == wtimeout.as<int32_t>());
    }
    if (auto ordered = requirements["ordered"]; ordered) {
        REQUIRE(event.command["ordered"].get_bool() == ordered.as<bool>());
    }
    if (auto bypass = requirements["bypassDocumentValidation"]; bypass) {
        REQUIRE(event.command["bypassDocumentValidation"].get_bool() == bypass.as<bool>());
    }
    if (auto readPref = requirements["$readPreference"]["mode"]; readPref) {
        REQUIRE(event.command["$readPreference"]["mode"].get_utf8().value ==
                readPref.as<std::string>());
    }
    if (auto staleness = requirements["$readPreference"]["maxStalenessSeconds"]) {
        REQUIRE(event.command["$readPreference"]["maxStalenessSeconds"].get_int64().value ==
                staleness.as<int64_t>());
    }
}

void requireAllEvents(mongocxx::pool::entry& client, ApmEvents events, YAML::Node requirements) {
    for (auto&& event : events) {
        requireEvent(event, requirements);
    }
}

void requireCollectionHasCount(mongocxx::pool::entry& client,
                               YAML::Node filterYaml,
                               long expected = 1,
                               std::string db = DEFAULT_DB,
                               std::string coll = DEFAULT_COLLECTION) {
    auto filter = genny::testing::toDocumentBson(filterYaml);
    INFO("Requiring " << expected << " document" << (expected == 1 ? "" : "s") << " in " << db
                      << "." << coll << " matching " << bsoncxx::to_json(filter.view()));
    long long actual = (*client)[db][coll].count_documents(filter.view());
    REQUIRE(actual == expected);
}

void requireCounts(mongocxx::pool::entry& client, YAML::Node outcomeData) {
    for (auto&& filterYaml : outcomeData) {
        requireCollectionHasCount(client, filterYaml);
    }
}

void requireOutcomeCounts(mongocxx::pool::entry& client, YAML::Node outcomeCounts) {
    for (auto&& countAssertion : outcomeCounts) {
        requireCollectionHasCount(
            client, countAssertion["Filter"], countAssertion["Count"].as<long>());
    }
}

NodeSource createConfigurationYaml(YAML::Node operations) {
    YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Phases:
            - Repeat: 1
          )");
    config["Actors"][0]["Database"] = DEFAULT_DB;
    config["Actors"][0]["Phases"][0]["Collection"] = DEFAULT_COLLECTION;
    config["Actors"][0]["Phases"][0]["Operations"] = operations;
    return NodeSource{YAML::Dump(config), "operationsConfig"};
}

void requireAfterState(mongocxx::pool::entry& client, ApmEvents& events, YAML::Node tcase) {
    if (auto ocd = tcase["OutcomeData"]; ocd) {
        requireCounts(client, ocd);
    }
    if (auto ocounts = tcase["OutcomeCounts"]; ocounts) {
        requireOutcomeCounts(client, ocounts);
    }
    if (auto requirements = tcase["ExpectAllEvents"]; requirements) {
        requireAllEvents(client, events, requirements);
    }
    if (auto expectCollections = tcase["ExpectedCollectionsExist"]; expectCollections) {
        requireExpectedCollectionsExist(client, events, expectCollections);
    }
}

}  // namespace

struct CrudActorTestCase {

    explicit CrudActorTestCase() = default;

    explicit CrudActorTestCase(YAML::Node node)
        : description{node["Description"].as<std::string>()},
          operations{node["Operations"]},
          runMode{convertRunMode(node)},
          error{node["Error"]},
          tcase{node} {}

    void doRun() const {
        try {
            auto events = ApmEvents{};
            auto apmCallback = makeApmCallback(events);

            auto config = createConfigurationYaml(operations);
            genny::ActorHelper ah(
                config.root(), 1, MongoTestFixture::connectionUri().to_string(), apmCallback);
            auto client = ah.client();
            dropAllDatabases(*client);
            events.clear();

            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            // assert on copies so tests that themselves query don't affect the events
            auto eventsCopy = events;

            if (runMode == RunMode::kExpectedSetupException ||
                runMode == RunMode::kExpectedRuntimeException) {
                FAIL("Expected exception " << error.as<std::string>() << " but not thrown");
            } else {
                requireAfterState(client, eventsCopy, tcase);
            }
        } catch (const boost::exception& e) {
            launderException(e);
        } catch (const std::exception& e) {
            launderException(e);
        }
    }

    template <typename X>
    void launderException(const X& e) const {
        auto diagInfo = boost::diagnostic_information(e);
        if (runMode == RunMode::kExpectedSetupException ||
            runMode == RunMode::kExpectedRuntimeException) {
            auto actual = boost::trim_copy(diagInfo);
            auto expect = boost::trim_copy(error.as<std::string>());
            INFO("Actual exception message : [[" << actual << "]]");
            REQUIRE_THAT(actual, MultilineMatch(expect));
        } else {
            INFO(description << " CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }

    void run() const {
        DYNAMIC_SECTION(description) {
            this->doRun();
        }
    }


    YAML::Node error;
    RunMode runMode = RunMode::kNormal;
    std::string description;
    YAML::Node operations;
    YAML::Node tcase;
};

}  // namespace genny::testing


namespace YAML {

template <>
struct convert<genny::testing::CrudActorTestCase> {
    static bool decode(YAML::Node node, genny::testing::CrudActorTestCase& out) {
        out = genny::testing::CrudActorTestCase(node);
        return true;
    }
};

}  // namespace YAML


namespace {
TEST_CASE("CrudActor YAML Tests",
          "[standalone][single_node_replset][three_node_replset][CrudActor]") {

    genny::testing::runTestCaseYaml<genny::testing::CrudActorTestCase>(
        "/src/cast_core/test/CrudActorYamlTests.yml");
}
}  // namespace
