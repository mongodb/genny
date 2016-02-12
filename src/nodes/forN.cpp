#include "forN.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

forN::forN(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "forN") {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have type == forN";
        exit(EXIT_FAILURE);
    }
    if (!ynode["node"]) {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have node entry";
        exit(EXIT_FAILURE);
    }
    N = ynode["N"].as<uint64_t>();
    auto yamlNode = ynode["node"];
    myNode = makeUniqeNode(yamlNode);
}

// Execute the node
void forN::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    chrono::high_resolution_clock::time_point start, stop;
    for (uint64_t i = 0; i < N && !(stopped || myState->stopped); i++) {
        BOOST_LOG_TRIVIAL(debug) << "In forN and executing interation " << i;
        start = chrono::high_resolution_clock::now();
        myNode->execute(myState);
        stop = chrono::high_resolution_clock::now();
        myNode->recordMicros(std::chrono::duration_cast<chrono::microseconds>(stop - start));
    }
}
std::pair<std::string, std::string> forN::generateDotGraph() {
    // Do default, and get any extra from the child node.
    // Don't use the graph from the child node itself since it's next node isn't used.
    return (std::pair<std::string, std::string>{name + " -> " + nextName + "\n",
                                                myNode->generateDotGraph().second});
}

void forN::logStats() {
    node::logStats();
    myNode->logStats();
}

bsoncxx::document::value forN::getStats(bool withReset) {
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    bsoncxx::builder::stream::document document{};

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that could be
    // moved into document
    auto stats = myStats.getStats(withReset);
    bsoncxx::builder::stream::document forNdocument{};
    auto forNStats = myNode->getStats(withReset);
    document << name << open_document << bsoncxx::builder::concatenate(stats.view())
             << bsoncxx::builder::concatenate(forNStats.view()) << close_document;
    return (document << bsoncxx::builder::stream::finalize);
}
}
