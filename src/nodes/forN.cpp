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
    for (uint64_t i = 0; i < N; i++) {
        BOOST_LOG_TRIVIAL(debug) << "In forN and executing interation " << i;
        start = chrono::high_resolution_clock::now();
        myNode->execute(myState);
        stop = chrono::high_resolution_clock::now();
        BOOST_LOG_TRIVIAL(debug) << "Node " << name << " took "
                                 << std::chrono::duration_cast<chrono::microseconds>(stop - start)
                                        .count() << " microseconds";
    }
}
}
