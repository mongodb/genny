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
#include "sleep.hpp"
#include "join.hpp"
#include "finish_node.hpp"
#include "workload.hpp"

using namespace mwg;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;

TEST_CASE("Nodes", "[nodes]") {
    unordered_map<string, bsoncxx::array::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::array::value> tvariables;  // thread variables

    workload myWorkload;
    auto state = shared_ptr<threadState>(
        new threadState(12234, tvariables, wvariables, myWorkload, "t", "c"));
    vector<shared_ptr<node>> vectornodes;
    unordered_map<string, shared_ptr<node>> nodes;

    SECTION("doAll") {
        auto doAllYaml = YAML::Load(R"yaml(
          name : doAll
          type : doAll
          childNodes :
            - thingA
            - thingB
          next : join # Next state of the doAll should be a join.
        )yaml");
        auto doAllNode = makeSharedNode(doAllYaml);
        nodes[doAllNode->getName()] = doAllNode;
        vectornodes.push_back(doAllNode);

        auto thing1Y = YAML::Load(R"yaml(
          name : thingA
          print : Thing A running
          type : noop
          next : join # Child thread continues until it gets to the join
            )yaml");
        auto thing1Node = makeSharedNode(thing1Y);
        nodes[thing1Node->getName()] = thing1Node;
        vectornodes.push_back(thing1Node);
        auto thing2Y = YAML::Load(R"yaml(
          name : thingB
          print : Thing B running
          type : noop
          next : join # Child thread continues until it gets to the join
            )yaml");
        auto thing2Node = makeSharedNode(thing2Y);
        nodes[thing2Node->getName()] = thing2Node;
        vectornodes.push_back(thing2Node);
        auto joinY = YAML::Load(R"yaml(
          name : join
          print : In Join
          type : join
          next : Finish
            )yaml");
        auto joinNode = makeSharedNode(joinY);
        nodes[joinNode->getName()] = joinNode;
        vectornodes.push_back(joinNode);
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode;
        vectornodes.push_back(mynode);
        // connect the nodes
        for (auto mnode : vectornodes) {
            mnode->setNextNode(nodes, vectornodes);
        }
        // execute
        state->currentNode = doAllNode;
        while (state->currentNode != nullptr)
            state->currentNode->executeNode(state);
        REQUIRE(doAllNode->getCount() == 1);
        REQUIRE(thing1Node->getCount() == 1);
        REQUIRE(thing2Node->getCount() == 1);
        REQUIRE(joinNode->getCount() == 1);
    }
    SECTION("Random") {
        auto randomYaml = YAML::Load(R"yaml(
          name : random
          type : random_choice
          next :
            thingA : 0.5
            thingB : 0.5
        )yaml");
        auto randomNode = makeSharedNode(randomYaml);
        nodes[randomNode->getName()] = randomNode;
        vectornodes.push_back(randomNode);

        auto thing1Y = YAML::Load(R"yaml(
          name : thingA
          print : Thing A running
          type : noop
          next : Finish
            )yaml");
        auto thing1Node = makeSharedNode(thing1Y);
        nodes[thing1Node->getName()] = thing1Node;
        vectornodes.push_back(thing1Node);
        auto thing2Y = YAML::Load(R"yaml(
          name : thingB
          print : Thing B running
          type : noop
          next : Finish
            )yaml");
        auto thing2Node = makeSharedNode(thing2Y);
        nodes[thing2Node->getName()] = thing2Node;
        vectornodes.push_back(thing2Node);
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode;
        vectornodes.push_back(mynode);
        // connect the nodes
        for (auto mnode : vectornodes) {
            mnode->setNextNode(nodes, vectornodes);
        }
        // execute
        state->currentNode = randomNode;
        while (state->currentNode != nullptr)
            state->currentNode->executeNode(state);
        REQUIRE((thing1Node->getCount() + thing2Node->getCount()) == 1);
    }
}
