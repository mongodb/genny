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
            for (auto entry : override) {
                overrides[entry.first.Scalar()] = makeUniqueValueGenerator(entry.second);
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
    for (auto override : overrides) {
        if (override.first.compare("database") == 0) {
            BOOST_LOG_TRIVIAL(trace) << "Setting database name in workloadnode";
            myWorkloadState.DBName = override.second->generateString(*myState);
        } else if (override.first.compare("collection") == 0) {
            BOOST_LOG_TRIVIAL(trace) << "Setting collection name in workloadnode";
            myWorkloadState.CollectionName = override.second->generateString(*myState);
        } else if (override.first.compare("name") == 0) {
            BOOST_LOG_TRIVIAL(trace) << "Setting workload name in workloadnode";
            myWorkload->name = override.second->generateString(*myState);
        } else if (override.first.compare("threads") == 0) {
            BOOST_LOG_TRIVIAL(trace) << "Setting number of threads in workloadnode";
            myWorkloadState.numParallelThreads = override.second->generateInt(*myState);
        } else if (override.first.compare("RunLength") == 0) {
            BOOST_LOG_TRIVIAL(trace) << "Setting runlength in workloadnode";
            myWorkloadState.runLengthMs = override.second->generateInt(*myState);
        } else {
            // Handle generic variables here
            BOOST_LOG_TRIVIAL(trace) << "Setting variable " << override.first << " in workloadnode";
            if (myWorkloadState.tvariables.count(override.first) > 0) {
                myWorkloadState.tvariables.find(override.first)->second =
                    override.second->generate(*myState);
                BOOST_LOG_TRIVIAL(trace) << "Setting existing tvariable " << override.first
                                         << " in workloadnode";
            } else if (myWorkloadState.wvariables.count(override.first) > 0) {
                myWorkloadState.wvariables.find(override.first)->second =
                    override.second->generate(*myState);
                BOOST_LOG_TRIVIAL(trace) << "Setting existing wvariable " << override.first
                                         << " in workloadnode";
            } else {
                myWorkloadState.tvariables.insert(
                    {override.first, override.second->generate(*myState)});
                BOOST_LOG_TRIVIAL(trace) << "Setting new tvariable " << override.first
                                         << " in workloadnode";
            }
        }
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
