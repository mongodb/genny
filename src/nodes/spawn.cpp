#include "spawn.hpp"

#include <thread>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

#include "workload.hpp"

namespace mwg {

Spawn::Spawn(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "spawn") {
        BOOST_LOG_TRIVIAL(fatal) << "Spawn constructor but yaml entry doesn't have type == Spawn";
        exit(EXIT_FAILURE);
    }

    // Get the spawn children nodes
    if (auto spawnNode = ynode["spawn"]) {
        if (spawnNode.IsScalar()) {
            nodeNames.push_back(spawnNode.Scalar());
        } else if (spawnNode.IsSequence()) {
            for (auto nnode : spawnNode) {
                nodeNames.push_back(nnode.Scalar());
            }
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Spawn constructor and spawn is a map";
            exit(EXIT_FAILURE);
        }
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "Spawn constructor and spawn doesn't exist";
        exit(EXIT_FAILURE);
    }
}

void Spawn::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                        vector<shared_ptr<node>>& vectornodesIn) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next node vector for Spawn node" << name
                             << ". Next node should be " << nextName;
    node::setNextNode(nodes, vectornodesIn);
    for (auto name : nodeNames) {
        spawnNodes.push_back(nodes[name]);
    }
}

// Execute the node
void Spawn::execute(shared_ptr<threadState> myState) {
    // Execute all the child nodes in their own threads. And wait in the join node.
    for (auto node : spawnNodes) {
        // setup a new thread state for this node
        auto newState = shared_ptr<threadState>(new threadState(myState->rng(),
                                                                myState->tvariables,
                                                                myState->wvariables,
                                                                myState->workloadState,
                                                                myState->DBName,
                                                                myState->CollectionName,
                                                                myState->workloadState.uri));
        startThread(node, newState)->detach();
    }
}

std::pair<std::string, std::string> Spawn::generateDotGraph() {
    string graph;
    for (string nextNodeN : nodeNames) {
        graph += name + " -> " + nextNodeN + ";\n";
    }
    graph += name + " -> " + nextName + ";\n";
    return (std::pair<std::string, std::string>{graph, ""});
}
}
