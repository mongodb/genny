#include "workloadNode.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>
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
                if (dbNameNode.IsScalar()) {
                    dbName = dbNameNode.Scalar();
                } else {
                    BOOST_LOG_TRIVIAL(fatal) << "Workload node override dbname is not a scalar";
                    exit(EXIT_FAILURE);
                }
            }
            if (auto collectionNode = override["collection"]) {
                if (collectionNode.IsScalar()) {
                    collectionName = collectionNode.Scalar();
                } else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Workload node override CollectionName is not a scalar";
                    exit(EXIT_FAILURE);
                }
            }
            if (auto workNameNode = override["name"]) {
                if (workNameNode.IsScalar()) {
                    workloadName = workNameNode.Scalar();
                } else {
                    BOOST_LOG_TRIVIAL(fatal) << "Workload node override name is not a scalar";
                    exit(EXIT_FAILURE);
                }
            }
            if (auto threadsNode = override["threads"]) {
                if (threadsNode.IsScalar()) {
                    numThreads = threadsNode.as<int64_t>();
                } else {
                    BOOST_LOG_TRIVIAL(fatal) << "Workload node override threads is not a scalar";
                    exit(EXIT_FAILURE);
                }
            }
            if (auto runLengthNode = override["runLength"]) {
                if (runLengthNode.IsScalar()) {
                    runLength = runLengthNode.as<int64_t>();
                } else {
                    BOOST_LOG_TRIVIAL(fatal) << "Workload node override runLength is not a scalar";
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Workload node overrides aren't a map";
            exit(EXIT_FAILURE);
        }
    }
}

// Execute the node
void workloadNode::execute(shared_ptr<threadState> myState) {
    myWorkload->uri = myState->myWorkload.uri;
    BOOST_LOG_TRIVIAL(debug) << "In workloadNode and executing";
    // set random seed based on current random seed.
    // should it be set in constructor? Is that safe?
    myWorkload->setRandomSeed(myState->rng());
    if (dbName) {
        myWorkload->getState().DBName = *dbName;
    }
    if (collectionName) {
        myWorkload->getState().CollectionName = *collectionName;
    }
    if (workloadName) {
        myWorkload->name = *workloadName;
    }
    if (numThreads) {
        myWorkload->numParallelThreads = *numThreads;
    }
    if (runLength) {
        myWorkload->runLength = *runLength;
    }
    myWorkload->execute();
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
