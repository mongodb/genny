#include "forN.hpp"
#include <stdlib.h>

namespace mwg {

forN::forN(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = forN
    if (!ynode) {
        cerr << "ForN constructor and !ynode" << endl;
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        cerr << "Not map in forN type initializer" << endl;
        exit(EXIT_FAILURE);
    }
    if (ynode["type"].Scalar() != "forN") {
        cerr << "ForN constructor but yaml entry doesn't have type == forN" << endl;
        exit(EXIT_FAILURE);
    }
    name = ynode["name"].Scalar();
    nextName = ynode["next"].Scalar();
    // cout << "In forN constructor. Name: " << name << ", nextName: " << nextName
    // << endl;
    // Need to read in the workload, or implicitly make one.
    if (!ynode["workload"]) {
        cerr << "ForN constructor but yaml entry doesn't have a workload entry" << endl;
        exit(EXIT_FAILURE);
    }
    N = ynode["N"].as<uint64_t>();
    auto yamlWorkload = ynode["workload"];
    myWorkload = workload(yamlWorkload);
}

// Execute the node
void forN::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    for (int i = 0; i < N; i++) {
        cout << "In forN and executing interation " << i << endl;
        myWorkload.execute(myState->conn);
    }
}
}
