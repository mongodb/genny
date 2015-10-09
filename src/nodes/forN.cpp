#include "forN.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

forN::forN(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "forN") {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have type == forN";
        exit(EXIT_FAILURE);
    }
    if (!ynode["workload"]) {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have a workload entry";
        exit(EXIT_FAILURE);
    }
    N = ynode["N"].as<uint64_t>();
    auto yamlWorkload = ynode["workload"];
    myWorkload = workload(yamlWorkload);
}

// Execute the node
void forN::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    for (uint64_t i = 0; i < N; i++) {
        BOOST_LOG_TRIVIAL(debug) << "In forN and executing interation " << i;
        myWorkload.execute(myState->conn);
    }
}
}
