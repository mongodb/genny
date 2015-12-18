#include "catch.hpp"
#include <bson.h>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>
#include "node.hpp"
#include "doAll.hpp"
#include "workload.hpp"

using namespace mwg;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;

TEST_CASE("Nodes", "[nodes]") {
    unordered_map<string, bsoncxx::types::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::types::value> tvariables;  // thread variables

    workload myWorkload;
    threadState state(12234, tvariables, wvariables, myWorkload, "t", "c");

    SECTION("doAll") {
        auto doAllNode = doAll(YAML::Load(R"yaml(
      name : doAll
      type : doAll
      childNodes : 
        - thingA
        - thingB
      next : join # Next state of the doAll should be a join.
    })yaml"));

        auto thing1 = YAML::Load(R"yaml(
      name : doAll
      type : doAll
      childNodes : 
        - thingA
        - thingB
      next : join # Next state of the doAll should be a join.
    })yaml";
    }
}
