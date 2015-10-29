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
    name = ynode["name"].Scalar();
    nextName = ynode["next"].Scalar();
    BOOST_LOG_TRIVIAL(debug) << "In node constructor. Name: " << name << ", nextName: " << nextName;
    if (ynode["print"]) {
        BOOST_LOG_TRIVIAL(debug) << "In node constructor and print";
        text = ynode["print"].Scalar();
    }
}

void node::setNextNode(unordered_map<string, shared_ptr<node>>& nodes) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node for " << name << ". Next node should be "
                             << nextName;
    nextNode = nodes[nextName];
}

void node::executeNextNode(shared_ptr<threadState> myState) {
    // execute the next node if there is one
    BOOST_LOG_TRIVIAL(debug) << "just executed " << name << ". NextName is " << nextName;
    auto next = nextNode.lock();
    if (!next)
        BOOST_LOG_TRIVIAL(error) << "nextNode is null for some reason";
    if (name != "Finish" && next && !stopped) {
        BOOST_LOG_TRIVIAL(debug) << "About to call nextNode->executeNode";
        // update currentNode in the state. Protect the reference while executing
        shared_ptr<node> me = myState->currentNode;
        myState->currentNode = next;
        next->executeNode(myState);
    }
}

void node::executeNode(shared_ptr<threadState> myState) {
    // execute this node
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

void runThread(shared_ptr<node> Node, shared_ptr<threadState> myState) {
    myState->currentNode = Node;
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
