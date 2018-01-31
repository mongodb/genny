#include "forN.hpp"
#include <boost/log/trivial.hpp>
#include <stdlib.h>

namespace mwg {

ForN::ForN(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "ForN") {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have type == ForN";
        exit(EXIT_FAILURE);
    }
    if (!ynode["node"]) {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have node entry";
        exit(EXIT_FAILURE);
    }
    N = IntOrValue(ynode["N"]);
    myNodeName = ynode["node"].Scalar();
}

// Execute the node
void ForN::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    chrono::high_resolution_clock::time_point start, stop;
    int64_t number = N.getInt(*myState);
    for (int64_t i = 0; i < number && !(stopped || myState->stopped); i++) {
        BOOST_LOG_TRIVIAL(debug) << "In ForN and executing interation " << i << " of " << number;
        myState->currentNode = myNode;
        while (myState->currentNode != nullptr) {
            myState->currentNode->executeNode(myState);
        }
    }
}
void ForN::setNextNode(unordered_map<string, node*>& nodes,
                       vector<shared_ptr<node>>& vectornodesIn) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node vector for ForN node" << name
                             << ". Next node should be " << nextName;
    node::setNextNode(nodes, vectornodesIn);
    myNode = nodes[myNodeName];
}

std::pair<std::string, std::string> ForN::generateDotGraph() {
    // Do default, and get any extra from the child node.
    // Don't use the graph from the child node itself since it's next node isn't used.
    return (std::pair<std::string, std::string>{
        name + " -> " + nextName + ";\n" + name + " -> " + myNodeName + ";\n", ""});
}

bsoncxx::document::value ForN::getStats(bool withReset) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::concatenate;
    bsoncxx::builder::basic::document document;

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that
    // could be
    // moved into document
    document.append(concatenate(myStats.getStats(withReset)));
    document.append(concatenate(myNode->getStats(withReset)));
    return make_document(kvp(name, document.view()));
}
}
