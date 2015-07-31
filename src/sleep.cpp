#include <stdio.h>
#include <stdlib.h>
#include "sleep.hpp"
#include <random>
#include "parse_util.hpp"
#include <time.h>

namespace mwg {
sleepNode::sleepNode(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = sleep
    if (!ynode) {
        cerr << "SleepNode constructor and !ynode" << endl;
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        cerr << "Not map in sleepNode type initializer" << endl;
        exit(EXIT_FAILURE);
    }
    if (ynode["type"].Scalar() != "sleep") {
        cerr << "SleepNode constructor but yaml entry doesn't have type == sleep" << endl;
        exit(EXIT_FAILURE);
    }
    name = ynode["name"].Scalar();
    sleeptime = ynode["sleep"].as<uint64_t>();
    cout << "In sleepNode constructor. Sleep time is " << sleeptime << endl;
    name = ynode["name"].Scalar();
    nextName = ynode["next"].Scalar();
}

// Execute the node
void sleepNode::execute(shared_ptr<threadState> myState) {
    cout << "sleepNode.execute. Sleeping for " << sleeptime << " ms" << endl;
    struct timespec t;
    t.tv_sec = (int)(sleeptime / 1000);
    t.tv_nsec = (sleeptime % 1000) * 1000 * 1000;
    struct timespec out;
    nanosleep(&t, &out);
    cout << "Slept." << endl;
}
}
