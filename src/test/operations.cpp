#include "catch.hpp"
#include "set_variable.hpp"
#include "workload.hpp"

#include <bson.h>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>

using namespace mwg;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;

TEST_CASE("Set Variables", "[variables]") {
    unordered_map<string, bsoncxx::types::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::types::value> tvariables;  // thread variables
    bsoncxx::types::b_int64 value;
    value.value = 1;
    wvariables.insert({"workloadVar", bsoncxx::types::value(value)});
    value.value = 2;
    tvariables.insert({"threadVar", bsoncxx::types::value(value)});
    mongocxx::client conn{mongocxx::uri{}};

    workload myWorkload;
    threadState state(12234, tvariables, wvariables, myWorkload, "t", "c");

    SECTION("Sanity check setup") {
        REQUIRE(state.wvariables.find("workloadVar")->second.get_int64().value == 1);
        REQUIRE(state.tvariables.find("threadVar")->second.get_int64().value == 2);
    }

    SECTION("Set existing thread variable") {
        auto yaml = YAML::Load(R"yaml(
    type : set_variable
    target : threadVar
    value : 3)yaml");

        auto testSet = set_variable(yaml);
        testSet.execute(conn, state);
        REQUIRE(state.tvariables.find("threadVar")->second.get_int64().value == 3);
        REQUIRE(state.wvariables.count("threadVar") ==
                0);  // make sure it didn't show up in the wrong place
    }
    SECTION("Set existing workload variable") {
        auto yaml = YAML::Load(R"yaml(
    type : set_variable
    target : workloadVar
    value : 3)yaml");

        auto testSet = set_variable(yaml);
        testSet.execute(conn, state);
        REQUIRE(state.wvariables.find("workloadVar")->second.get_int64().value == 3);
        REQUIRE(state.tvariables.count("workloadVar") ==
                0);  // make sure it didn't show up in the wrong place
    }
    SECTION("Set non-existing variable") {
        auto yaml = YAML::Load(R"yaml(
    type : set_variable
    target : newVar
    value : 4)yaml");

        auto testSet = set_variable(yaml);
        testSet.execute(conn, state);
        REQUIRE(state.tvariables.find("newVar")->second.get_int64().value == 4);
        REQUIRE(state.wvariables.count("newVar") ==
                0);  // make sure it didn't show up in the wrong place
    }
    SECTION("Set a string value to non-existant variable") {
        auto yaml = YAML::Load(R"yaml(
        type : set_variable
        target : newStringVar
        value : test_string)yaml");

        auto testSet = set_variable(yaml);
        string expected = "test_string";
        testSet.execute(conn, state);
        auto actual = state.tvariables.find("newStringVar")->second.get_utf8().value;
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << actual << "\"");
        REQUIRE(actual.compare(expected) == 0);
        REQUIRE(state.wvariables.count("newStringVar") ==
                0);  // make sure it didn't show up in the wrong place
    }
    SECTION("Set a string value to existing int") {
        auto yaml = YAML::Load(R"yaml(
        type : set_variable
        target : threadVar
        value : test_string)yaml");

        auto testSet = set_variable(yaml);
        string expected = "test_string";
        testSet.execute(conn, state);
        auto actual = state.tvariables.find("threadVar")->second;
        REQUIRE(actual.type() == bsoncxx::type::k_utf8);
        auto actualval = actual.get_utf8().value;
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << actualval << "\"");
        REQUIRE(actualval.compare(expected) == 0);
        REQUIRE(state.wvariables.count("threadVar") ==
                0);  // make sure it didn't show up in the wrong place
    }

    SECTION("Set DBName") {
        auto yaml = YAML::Load(R"yaml(
        type : set_variable
        target : DBName
        value : NewDB)yaml");
        // Original database name
        REQUIRE(state.DBName.compare("t") == 0);
        auto testSet = set_variable(yaml);
        string expected = "NewDB";
        testSet.execute(conn, state);
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << state.DBName << "\"");
        REQUIRE(state.DBName.compare(expected) == 0);
        REQUIRE(state.wvariables.count("DBName") ==
                0);  // make sure it didn't show up in the wrong place
        REQUIRE(state.tvariables.count("DBName") ==
                0);  // make sure it didn't show up in the wrong place
        REQUIRE(state.CollectionName.compare("c") == 0);
    }

    SECTION("Set CollectionName") {
        auto yaml = YAML::Load(R"yaml(
        type : set_variable
        target : CollectionName
        value : NewColl)yaml");
        // Original database name
        REQUIRE(state.DBName.compare("t") == 0);
        auto testSet = set_variable(yaml);
        string expected = "NewColl";
        testSet.execute(conn, state);
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << state.CollectionName << "\"");
        REQUIRE(state.CollectionName.compare(expected) == 0);
        REQUIRE(state.wvariables.count("CollectionName") ==
                0);  // make sure it didn't show up in the wrong place
        REQUIRE(state.tvariables.count("CollectionName") ==
                0);  // make sure it didn't show up in the wrong place
        REQUIRE(state.DBName.compare("t") == 0);
    }

    SECTION("Set from tvariable") {
        auto yaml = YAML::Load(R"yaml(
    type : set_variable
    target : workloadVar
    donorVariable : threadVar)yaml");

        auto testSet = set_variable(yaml);
        testSet.execute(conn, state);
        REQUIRE(state.wvariables.find("workloadVar")->second.get_int64().value == 2);
        REQUIRE(state.tvariables.find("threadVar")->second.get_int64().value == 2);
    }
    SECTION("Set from wvariable") {
        auto yaml = YAML::Load(R"yaml(
    type : set_variable
    target : threadVar
    donorVariable : workloadVar)yaml");

        auto testSet = set_variable(yaml);
        testSet.execute(conn, state);
        REQUIRE(state.tvariables.find("threadVar")->second.get_int64().value == 1);
        REQUIRE(state.wvariables.find("workloadVar")->second.get_int64().value == 1);
    }
}
