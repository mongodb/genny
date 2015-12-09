#include "node.hpp"
#include <boost/log/trivial.hpp>

#include "opNode.hpp"
#include "random_choice.hpp"
#include "sleep.hpp"
#include "finish_node.hpp"
#include "forN.hpp"
#include "doAll.hpp"
#include "join.hpp"
#include "workloadNode.hpp"

namespace mwg {

static int count = 0;

node::node(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!ynode) {
        BOOST_LOG_TRIVIAL(fatal) << "Find constructor and !ynode";
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find type initializer";
        exit(EXIT_FAILURE);
    }
    if (ynode["name"])
        name = ynode["name"].Scalar();
    else
        name = ynode["type"].Scalar() + std::to_string(count++);  // default name
    if (ynode["next"])
        nextName = ynode["next"].Scalar();
    BOOST_LOG_TRIVIAL(debug) << "In node constructor. Name: " << name << ", nextName: " << nextName;
    if (ynode["print"]) {
        BOOST_LOG_TRIVIAL(debug) << "In node constructor and print";
        text = ynode["print"].Scalar();
    }
}

void node::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                       vector<shared_ptr<node>>& vectornodes) {
    if (nextName.empty()) {
        // Need default next node.
        // Check if there's a next in the vectornodes. Use that if there is
        // Otherwise use finish node
        BOOST_LOG_TRIVIAL(trace) << "nextName is empty. Using default values";
        // FIXME: The following code is a hack to find the next node in the list
        for (uint i = 0; i < vectornodes.size(); i++)
            if (vectornodes[i].get() == this) {
                BOOST_LOG_TRIVIAL(trace) << "Found node";
                if (i < vectornodes.size() - 1) {
                    BOOST_LOG_TRIVIAL(trace) << "Setting next node to next node in list";
                    auto next = vectornodes[i + 1];
                    nextName = next->name;
                    nextNode = next;
                } else {
                    BOOST_LOG_TRIVIAL(trace)
                        << "Node was last in vector. Setting next node to Finish";
                    nextName = "Finish";
                    nextNode = nodes["Finish"];
                }
                return;
            }
        BOOST_LOG_TRIVIAL(fatal) << "In setNextNode and fell through default value";
        exit(0);
    } else {
        nextNode = nodes[nextName];
    }
}

void node::executeNextNode(shared_ptr<threadState> myState) {
    // execute the next node if there is one
    BOOST_LOG_TRIVIAL(debug) << "just executed " << name << ". NextName is " << nextName;
    auto next = nextNode.lock();
    if (!next) {
        BOOST_LOG_TRIVIAL(fatal) << "nextNode is null for some reason";
        exit(0);
    }
    if (stopped) {
        BOOST_LOG_TRIVIAL(error) << "Stopped set";
        return;
    }
    if (name != "Finish" && next) {
        BOOST_LOG_TRIVIAL(debug) << "About to call nextNode->executeNode";
        // update currentNode in the state. Protect the reference while executing
        shared_ptr<node> me = myState->currentNode;
        myState->currentNode = next;
        next->executeNode(myState);
    } else {
        BOOST_LOG_TRIVIAL(debug) << "Next node is not null, but didn't execute it";
    }
}

void node::executeNode(shared_ptr<threadState> myState) {
    // execute this node
    BOOST_LOG_TRIVIAL(trace) << "node::executeNode. Name is " << name;
    chrono::high_resolution_clock::time_point start, stop;
    start = chrono::high_resolution_clock::now();
    execute(myState);
    stop = chrono::high_resolution_clock::now();
    BOOST_LOG_TRIVIAL(debug) << "Node " << name << " took "
                             << std::chrono::duration_cast<chrono::microseconds>(stop - start)
                                    .count() << " microseconds";
    if (!text.empty()) {
        BOOST_LOG_TRIVIAL(info) << text;
    }
    // execute the next node if there is one
    executeNextNode(myState);
}

std::pair<std::string, std::string> node::generateDotGraph() {
    return (std::pair<std::string, std::string>{name + " -> " + nextName + ";\n", ""});
}

void runThread(shared_ptr<node> Node, shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(trace) << "Node runThread";
    myState->currentNode = Node;
    BOOST_LOG_TRIVIAL(trace) << "Set node. Name is " << Node->name;
    Node->executeNode(myState);
}
node* makeNode(YAML::Node yamlNode) {
    if (!yamlNode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Node in makeNode is not a yaml map";
        exit(EXIT_FAILURE);
    }
    if (yamlNode["type"].Scalar() == "opNode") {
        return new opNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "random_choice") {
        return new random_choice(yamlNode);
    } else if (yamlNode["type"].Scalar() == "sleep") {
        return new sleepNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "forN") {
        return new forN(yamlNode);
    } else if (yamlNode["type"].Scalar() == "finish") {
        return new finishNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "doAll") {
        return new doAll(yamlNode);
    } else if (yamlNode["type"].Scalar() == "join") {
        return new join(yamlNode);
    } else if (yamlNode["type"].Scalar() == "workloadNode") {
        return new workloadNode(yamlNode);
    } else {
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor. Defaulting to opNode";
        return new opNode(yamlNode);
    }
}
unique_ptr<node> makeUniqeNode(YAML::Node yamlNode) {
    return unique_ptr<node>(makeNode(yamlNode));
}
shared_ptr<node> makeSharedNode(YAML::Node yamlNode) {
    return shared_ptr<node>(makeNode(yamlNode));
}
}
