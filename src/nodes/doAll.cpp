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
    if (ynode["backgroundNodes"]) {
        if (ynode["backgroundNodes"].IsSequence()) {
            for (auto nnode : ynode["backgroundNodes"]) {
                backgroundNodeNames.push_back(nnode.Scalar());
            }
        } else
            BOOST_LOG_TRIVIAL(fatal)
                << "doAll constructor and have backgroundNodes but not a sequence";
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
    for (auto name : backgroundNodeNames) {
        vectorbackground.push_back(nodes[name]);
    }
}

// Execute the node
void doAll::execute(shared_ptr<threadState> myState) {
    // Execute all the child nodes in their own threads. And wait in the join node.
    for (auto node : vectornodes) {
        // setup a new thread state for this node
        auto newState = shared_ptr<threadState>(new threadState(myState->rng(),
                                                                myState->tvariables,
                                                                myState->wvariables,
                                                                myState->myWorkload,
                                                                myState->DBName,
                                                                myState->CollectionName,
                                                                myState->myWorkload.uri));
        newState->parentThread = myState;
        myState->childThreadStates.push_back(newState);
        newState->myThread = shared_ptr<thread>(new thread(runThread, node, newState));
        myState->childThreads.push_back(newState->myThread);
    }
    for (auto node : vectorbackground) {
        // setup a new thread state for this node
        auto newState = shared_ptr<threadState>(new threadState(myState->rng(),
                                                                myState->tvariables,
                                                                myState->wvariables,
                                                                myState->myWorkload,
                                                                myState->DBName,
                                                                myState->CollectionName,
                                                                myState->myWorkload.uri));
        newState->parentThread = myState;
        myState->backgroundThreadStates.push_back(newState);
        newState->myThread = shared_ptr<thread>(new thread(runThread, node, newState));
        myState->backgroundThreads.push_back(newState->myThread);
    }
}

std::pair<std::string, std::string> doAll::generateDotGraph() {
    string graph;
    for (string nextNodeN : nodeNames) {
        graph += name + " -> " + nextNodeN + ";\n";
    }
    graph += name + " -> " + nextName + ";\n";
    return (std::pair<std::string, std::string>{graph, ""});
}
}
