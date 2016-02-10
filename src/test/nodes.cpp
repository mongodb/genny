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
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>


#include "node.hpp"
#include "doAll.hpp"
#include "sleep.hpp"
#include "join.hpp"
#include "finish_node.hpp"
#include "workload.hpp"
#include "workloadNode.hpp"
#include "spawn.hpp"

using namespace mwg;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;

namespace logging = boost::log;


// Class with extra accessors for test purposes
class TestWorkloadNode : public workloadNode {
public:
    TestWorkloadNode(YAML::Node& ynode) : workloadNode(ynode) {}
    workload& getWorkload() {
        return *myWorkload;
    };  // read only access to embedded workload
};


TEST_CASE("Nodes", "[nodes]") {
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
    unordered_map<string, bsoncxx::array::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::array::value> tvariables;  // thread variables

    workload myWorkload;
    auto workloadState = myWorkload.newWorkloadState();
    auto state = shared_ptr<threadState>(
        new threadState(12234, tvariables, wvariables, workloadState, "t", "c"));
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

    SECTION("spawn") {
        auto spawnYaml = YAML::Load(R"yaml(
          name : spawn
          type : spawn
          spawn :
            - thingA
            - thingB
          next : Finish # Next state of the spawn should be a join.
        )yaml");
        auto spawnNode = makeSharedNode(spawnYaml);
        nodes[spawnNode->getName()] = spawnNode;
        vectornodes.push_back(spawnNode);

        auto thing1Y = YAML::Load(R"yaml(
          name : thingA
          print : Thing A running
          type : noop
          next : Finish # Child thread continues until it gets to the join
            )yaml");
        auto thing1Node = makeSharedNode(thing1Y);
        nodes[thing1Node->getName()] = thing1Node;
        vectornodes.push_back(thing1Node);
        auto thing2Y = YAML::Load(R"yaml(
          name : thingB
          print : Thing B running
          type : noop
          next : Finish # Child thread continues until it gets to the join
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
        state->currentNode = spawnNode;
        while (state->currentNode != nullptr)
            state->currentNode->executeNode(state);
        REQUIRE(spawnNode->getCount() == 1);
        REQUIRE(thing1Node->getCount() == 1);
        REQUIRE(thing2Node->getCount() == 1);
    }

    SECTION("WorkloadNode") {
        INFO("WorkloadNode section")
        auto workloadNodeYAML = YAML::Load(R"yaml(
      type : workloadNode
      overrides : # These setting override what is set in the embedded workload
        threads : 4
        database : testDB2
        collection : testCollection2
        runLength : 10
        name : NewName
      workload :
        name : embeddedWorkload
        database : testDB1
        collection : testCollection1
        runLength : 5
        threads : 5
        nodes :
          - type : sleep
            sleep : 1
            print : In sleep
        )yaml");
        auto workNode = make_shared<TestWorkloadNode>(workloadNodeYAML);
        nodes[workNode->getName()] = workNode;
        vectornodes.push_back(workNode);
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode;
        vectornodes.push_back(mynode);
        // connect the nodes
        for (auto mnode : vectornodes) {
            mnode->setNextNode(nodes, vectornodes);
        }

        // this test is very limited now. The workload changes are embedded within the execute call.
        // Would need to check stats to see what database, collection, runLength, etc.

        // test that workload has embedded values.
        // REQUIRE(workNode->getWorkload().getState().DBName == "testDB1");
        // REQUIRE(workNode->getWorkload().getState().CollectionName == "testCollection1");
        // REQUIRE(workNode->getWorkload().runLength == 5);
        // REQUIRE(workNode->getWorkload().name == "embeddedWorkload");
        // REQUIRE(workNode->getWorkload().numParallelThreads == 5);
        workNode->executeNode(state);
        // REQUIRE(workNode->getWorkload().getState().DBName == "testDB2");
        // REQUIRE(workNode->getWorkload().getState().CollectionName == "testCollection2");
        // REQUIRE(workNode->getWorkload().runLength == 10);
        // REQUIRE(workNode->getWorkload().name == "NewName");
        // REQUIRE(workNode->getWorkload().numParallelThreads == 4);
    }

    SECTION("WorkloadNodeVariables") {
        INFO("WorkloadNode Variable section")
        auto workloadNodeYAML = YAML::Load(R"yaml(
      type : workloadNode
      overrides : # These setting override what is set in the embedded workload
        database :
          type : usevar
          variable : dbname
        collection : 
          type : usevar
          variable : collectionname
        runLength : 
          type : usevar
          variable : runlength
        name : NewName
        threads : 
          type : increment
          variable : nthreads
      workload :
        name : embeddedWorkload
        database : testDB1
        collection : testCollection1
        runLength : 5
        threads : 5
        nodes :
          - type : sleep
            sleep : 1
            print : In sleep
        )yaml");
        auto workNode = make_shared<TestWorkloadNode>(workloadNodeYAML);
        nodes[workNode->getName()] = workNode;
        vectornodes.push_back(workNode);
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode;
        vectornodes.push_back(mynode);
        // connect the nodes
        for (auto mnode : vectornodes) {
            mnode->setNextNode(nodes, vectornodes);
        }
        state->tvariables.insert(
            {"dbname", bsoncxx::builder::stream::array() << "vardbname" << finalize});
        state->tvariables.insert(
            {"collectionname",
             bsoncxx::builder::stream::array() << "varcollectionname" << finalize});
        state->tvariables.insert({"runlength", bsoncxx::builder::stream::array() << 6 << finalize});
        state->wvariables.insert({"nthreads", bsoncxx::builder::stream::array() << 7 << finalize});

        // test that workload has embedded values.
        // REQUIRE(workNode->getWorkload().getState().DBName == "testDB1");
        // REQUIRE(workNode->getWorkload().getState().CollectionName == "testCollection1");
        // REQUIRE(workNode->getWorkload().runLength == 5);
        // REQUIRE(workNode->getWorkload().name == "embeddedWorkload");
        // REQUIRE(workNode->getWorkload().numParallelThreads == 5);
        workNode->executeNode(state);
        // REQUIRE(workNode->getWorkload().getState().DBName == "vardbname");
        // REQUIRE(workNode->getWorkload().getState().CollectionName == "varcollectionname");
        // REQUIRE(workNode->getWorkload().runLength == 6);
        // REQUIRE(workNode->getWorkload().name == "NewName");
        // REQUIRE(workNode->getWorkload().numParallelThreads == 7);
        // workNode->executeNode(state);
        // REQUIRE(workNode->getWorkload().getState().DBName == "vardbname");
        // REQUIRE(workNode->getWorkload().getState().CollectionName == "varcollectionname");
        // REQUIRE(workNode->getWorkload().runLength == 6);
        // REQUIRE(workNode->getWorkload().name == "NewName");
        // REQUIRE(workNode->getWorkload().numParallelThreads == 8);
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
    SECTION("ifNode") {
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
        bsoncxx::types::b_int64 value;
        SECTION("equality") {
            auto ifNodeYaml = YAML::Load(R"yaml(
          name : ifNode
          type : ifNode
          ifNode : thingA
          elseNode : thingB
          comparison : 
              value : 1
              test : equals
        )yaml");
            auto ifNodeNode = makeSharedNode(ifNodeYaml);
            nodes[ifNodeNode->getName()] = ifNodeNode;
            vectornodes.push_back(ifNodeNode);

            // connect the nodes
            for (auto mnode : vectornodes) {
                mnode->setNextNode(nodes, vectornodes);
            }

            SECTION("true") {
                value.value = 1;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate true and take the ifnode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 1);
                REQUIRE(thing2Node->getCount() == 0);
            }
            SECTION("false") {
                value.value = 5;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate false and take the elsenode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 0);
                REQUIRE(thing2Node->getCount() == 1);
            }
        }

        SECTION("Greater") {
            auto ifNodeYaml = YAML::Load(R"yaml(
          name : ifNode
          type : ifNode
          ifNode : thingA
          elseNode : thingB
          comparison : 
              value : 1
              test : greater
        )yaml");
            auto ifNodeNode = makeSharedNode(ifNodeYaml);
            nodes[ifNodeNode->getName()] = ifNodeNode;
            vectornodes.push_back(ifNodeNode);

            // connect the nodes
            for (auto mnode : vectornodes) {
                mnode->setNextNode(nodes, vectornodes);
            }

            bsoncxx::types::b_int64 value;
            // execute -- if should evaluate false and take the ifnode
            state->currentNode = ifNodeNode;
            SECTION("true") {
                value.value = 5;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 1);
                REQUIRE(thing2Node->getCount() == 0);
            }
            SECTION("false") {
                value.value = 1;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate false and take the elsenode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 0);
                REQUIRE(thing2Node->getCount() == 1);
            }
        }
        SECTION("Less Than") {
            auto ifNodeYaml = YAML::Load(R"yaml(
          name : ifNode
          type : ifNode
          ifNode : thingA
          elseNode : thingB
          comparison : 
              value : 1
              test : less
        )yaml");
            auto ifNodeNode = makeSharedNode(ifNodeYaml);
            nodes[ifNodeNode->getName()] = ifNodeNode;
            vectornodes.push_back(ifNodeNode);

            // connect the nodes
            for (auto mnode : vectornodes) {
                mnode->setNextNode(nodes, vectornodes);
            }

            bsoncxx::types::b_int64 value;
            // execute -- if should evaluate false and take the ifnode
            state->currentNode = ifNodeNode;
            SECTION("true") {
                value.value = 0;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 1);
                REQUIRE(thing2Node->getCount() == 0);
            }
            SECTION("false") {
                value.value = 1;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate false and take the elsenode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 0);
                REQUIRE(thing2Node->getCount() == 1);
            }
        }
        SECTION("Greater or Equal") {
            auto ifNodeYaml = YAML::Load(R"yaml(
          name : ifNode
          type : ifNode
          ifNode : thingA
          elseNode : thingB
          comparison : 
              value : 1
              test : greater_or_equal
        )yaml");
            auto ifNodeNode = makeSharedNode(ifNodeYaml);
            nodes[ifNodeNode->getName()] = ifNodeNode;
            vectornodes.push_back(ifNodeNode);

            // connect the nodes
            for (auto mnode : vectornodes) {
                mnode->setNextNode(nodes, vectornodes);
            }

            bsoncxx::types::b_int64 value;
            // execute -- if should evaluate false and take the ifnode
            state->currentNode = ifNodeNode;
            SECTION("true") {
                value.value = 1;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 1);
                REQUIRE(thing2Node->getCount() == 0);
            }
            SECTION("false") {
                value.value = 0;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate false and take the elsenode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 0);
                REQUIRE(thing2Node->getCount() == 1);
            }
        }
        SECTION("Less or Equal") {
            auto ifNodeYaml = YAML::Load(R"yaml(
          name : ifNode
          type : ifNode
          ifNode : thingA
          elseNode : thingB
          comparison : 
              value : 1
              test : less_or_equal
        )yaml");
            auto ifNodeNode = makeSharedNode(ifNodeYaml);
            nodes[ifNodeNode->getName()] = ifNodeNode;
            vectornodes.push_back(ifNodeNode);

            // connect the nodes
            for (auto mnode : vectornodes) {
                mnode->setNextNode(nodes, vectornodes);
            }

            bsoncxx::types::b_int64 value;
            // execute -- if should evaluate false and take the ifnode
            state->currentNode = ifNodeNode;
            SECTION("true") {
                value.value = 1;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 1);
                REQUIRE(thing2Node->getCount() == 0);
            }
            SECTION("false") {
                value.value = 2;
                state->result = bsoncxx::builder::stream::array()
                    << value << bsoncxx::builder::stream::finalize;
                // execute -- if should evaluate false and take the elsenode
                state->currentNode = ifNodeNode;
                while (state->currentNode != nullptr)
                    state->currentNode->executeNode(state);
                REQUIRE(ifNodeNode->getCount() == 1);
                REQUIRE(thing1Node->getCount() == 0);
                REQUIRE(thing2Node->getCount() == 1);
            }
        }
    }
}
