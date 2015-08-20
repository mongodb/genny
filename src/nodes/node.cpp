#include "node.hpp"
#include <boost/log/trivial.hpp>

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
        text = ynode["print"].Scalar();
    }
}

void node::setNextNode(unordered_map<string, shared_ptr<node>>& nodes) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node for " << name << ". Next node should be "
                             << nextName;
    nextNode = nodes[nextName];
}

void node::executeNode(shared_ptr<threadState> myState) {
    // execute this node
    execute(myState);
    if (!text.empty()) {
        BOOST_LOG_TRIVIAL(info) << text;
    }
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
}
