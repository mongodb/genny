#include "doAll.hpp"
#include <thread>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

doAll::doAll(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "doAll") {
        BOOST_LOG_TRIVIAL(fatal) << "doAll constructor but yaml entry doesn't have type == doAll";
        exit(EXIT_FAILURE);
    }
    if (!ynode["childNodes"].IsSequence())
        BOOST_LOG_TRIVIAL(fatal)
            << "doAll constructor and childNodes doesn't exist or isn't a sequence";
    for (auto nnode : ynode["childNodes"]) {
        nodeNames.push_back(nnode.Scalar());
    }
}

void doAll::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                        vector<shared_ptr<node>>& vectornodesIn) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node vector for doAll node" << name
                             << ". Next node should be " << nextName;
    nextNode = nodes[nextName];
    for (auto name : nodeNames) {
        vectornodes.push_back(nodes[name]);
    }
}

// Execute the node
void doAll::execute(shared_ptr<threadState> myState) {
    // execute the workload N times
    // Execute all the child nodes in their own threads. And wait in the join node. Move to the join
    // node.
    // setup a vector of threads (in the workload)
    for (auto node : vectornodes) {
        // setup a new thread state for this node
        auto newState = shared_ptr<threadState>(new threadState(myState->rng(),
                                                                myState->tvariables,
                                                                myState->wvariables,
                                                                myState->myWorkload,
                                                                myState->DBName,
                                                                myState->CollectionName));
        newState->parentThread = myState;
        myState->childThreadStates.push_back(newState);
        newState->myThread = shared_ptr<thread>(new thread(runThread, node, newState));
        myState->childThreads.push_back(newState->myThread);
    }
}
}
