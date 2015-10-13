#include "join.hpp"
#include <thread>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

join::join(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "join") {
        BOOST_LOG_TRIVIAL(fatal) << "join constructor but yaml entry doesn't have type == join";
        exit(EXIT_FAILURE);
    }
}

// Execute the node
void join::executeNode(shared_ptr<threadState> myState) {
    // execute the workload N times
    chrono::high_resolution_clock::time_point start, stop;
    // Determine if I'm the parent or the child
    if (myState->childThreads.empty()) {
        // I'm the child
        // Child should end here. Return and end the thread. Will it return all the way up? I think
        // so
        BOOST_LOG_TRIVIAL(debug) << "Join node " << name << " for child thread. Returning";
        return;
    } else {
        // I'm the parent
        // Join across all the threads
        start = chrono::high_resolution_clock::now();
        BOOST_LOG_TRIVIAL(debug) << "Join node " << name
                                 << " is parent and entering join loop. Waiting for "
                                 << myState->childThreads.size() << " threads";
        // Wait for all the children to finish
        for (auto child : myState->childThreads) {
            child->join();
        }
        stop = chrono::high_resolution_clock::now();
        BOOST_LOG_TRIVIAL(debug) << "Join node " << name << " took "
                                 << std::chrono::duration_cast<chrono::milliseconds>(stop - start)
                                        .count() << " milliseconds";
        // clean up
        myState->childThreads.clear();
        myState->childThreadStates.clear();
        executeNextNode(myState);
    }
}
}
