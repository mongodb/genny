#include "workloadNode.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <stdlib.h>

#include "use_var_generator.hpp"
#include "workload.hpp"

namespace mwg {

workloadNode::workloadNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "workloadNode") {
        BOOST_LOG_TRIVIAL(fatal)
            << "workloadNode constructor but yaml entry doesn't have type == workloadNode";
        exit(EXIT_FAILURE);
    }
    if (auto yamlWorkload = ynode["workload"]) {
        myWorkload = unique_ptr<workload>(new workload(yamlWorkload));
    } else {
        BOOST_LOG_TRIVIAL(fatal)
            << "workloadNode constructor but yaml entry doesn't have a workload entry";
        exit(EXIT_FAILURE);
    }
    if (auto override = ynode["overrides"]) {
        if (override.IsMap()) {
            if (auto dbNameNode = override["database"]) {
                dbName = makeUniqueValueGenerator(dbNameNode);
            }
            if (auto collectionNode = override["collection"]) {
                collectionName = makeUniqueValueGenerator(collectionNode);
            }
            if (auto workNameNode = override["name"]) {
                workloadName = makeUniqueValueGenerator(workNameNode);
            }
            if (auto threadsNode = override["threads"]) {
                numThreads = makeUniqueValueGenerator(threadsNode);
            }
            if (auto runLengthMsNode = override["runLengthMs"]) {
                runLengthMs = makeUniqueValueGenerator(runLengthMsNode);
            }
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Workload node overrides aren't a map";
            exit(EXIT_FAILURE);
        }
    }
}

// Execute the node
void workloadNode::execute(shared_ptr<threadState> myState) {
    WorkloadExecutionState myWorkloadState = myWorkload->newWorkloadState();
    myWorkloadState.uri = myState->workloadState->uri;
    BOOST_LOG_TRIVIAL(debug) << "In workloadNode and executing";
    // set random seed based on current random seed.
    // should it be set in constructor? Is that safe?
    myWorkload->setRandomSeed(myState->rng(), &myWorkloadState);
    if (dbName) {
        myWorkloadState.DBName = dbName->generateString(*myState);
    }
    if (collectionName) {
        myWorkloadState.CollectionName = collectionName->generateString(*myState);
    }
    if (workloadName) {
        myWorkload->name = workloadName->generateString(*myState);
    }
    if (numThreads) {
        myWorkloadState.numParallelThreads = numThreads->generateInt(*myState);
    }
    if (runLengthMs) {
        myWorkloadState.runLengthMs = runLengthMs->generateInt(*myState);
    }
    myWorkload->execute(&myWorkloadState);
}
std::pair<std::string, std::string> workloadNode::generateDotGraph() {
    return (std::pair<std::string, std::string>{name + " -> " + nextName + ";\n",
                                                myWorkload->generateDotGraph()});
}
void workloadNode::logStats() {
    myWorkload->logStats();
}
bsoncxx::document::value workloadNode::getStats(bool withReset) {
    return (myWorkload->getStats(withReset));
}

void workloadNode::stop() {
    stopped = true;
    myWorkload->stop();
}
}
