#include "node.hpp"

namespace mwg {

node::node(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!ynode) {
        cerr << "Find constructor and !ynode" << endl;
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        cerr << "Not map in find type initializer" << endl;
        exit(EXIT_FAILURE);
    }
    name = ynode["name"].Scalar();
    nextName = ynode["next"].Scalar();
    // cout << "In node constructor. Name: " << name << ", nextName: " << nextName << endl;
}

void node::setNextNode(unordered_map<string, shared_ptr<node>>& nodes) {
    // cout << "Setting next node for " << name << ". Next node should be " << nextName << endl;
    nextNode = nodes[nextName];
}

void node::executeNode(shared_ptr<threadState> myState) {
    // execute this node
    execute(myState);
    // execute the next node if there is one
    // cout << "just executed " << name << ". NextName is " << nextName << endl;
    auto next = nextNode.lock();
    if (!next)
        cerr << "nextNode is null for some reason" << endl;
    if (name != "Finish" && next && !stopped) {
        // cout << "About to call nextNode->executeNode" << endl;
        // update currentNode in the state. Protect the reference while executing
        shared_ptr<node> me = myState->currentNode;
        myState->currentNode = next;
        next->executeNode(myState);
    }
}
}
