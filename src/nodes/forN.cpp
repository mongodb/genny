#include "forN.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>

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
    N = ynode["N"].as<uint64_t>();
    myNodeName = ynode["node"].Scalar();
}

// Execute the node
void ForN::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    chrono::high_resolution_clock::time_point start, stop;
    for (uint64_t i = 0; i < N && !(stopped || myState->stopped); i++) {
        BOOST_LOG_TRIVIAL(debug) << "In ForN and executing interation " << i;
        myState->currentNode = myNode;
        while (myState->currentNode != nullptr) {
            myState->currentNode->executeNode(myState);
        }
    }
}
void ForN::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                       vector<shared_ptr<node>>& vectornodesIn) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node vector for ForN node" << name
                             << ". Next node should be " << nextName;
    nextNode = nodes[nextName];
    myNode = nodes[myNodeName];
}

std::pair<std::string, std::string> ForN::generateDotGraph() {
    // Do default, and get any extra from the child node.
    // Don't use the graph from the child node itself since it's next node isn't used.
    return (std::pair<std::string, std::string>{
        name + " -> " + nextName + ";\n" + name + " -> " + myNodeName + ";\n", ""});
}

bsoncxx::document::value ForN::getStats(bool withReset) {
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    bsoncxx::builder::stream::document document{};

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that
    // could be
    // moved into document
    auto stats = myStats.getStats(withReset);
    bsoncxx::builder::stream::document ForNdocument{};
    auto forNStats = myNode->getStats(withReset);
    document << name << open_document << bsoncxx::builder::concatenate(stats.view())
             << bsoncxx::builder::concatenate(forNStats.view()) << close_document;
    return (document << bsoncxx::builder::stream::finalize);
}
}
