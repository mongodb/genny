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
    if (!ynode["workload"]) {
        BOOST_LOG_TRIVIAL(fatal)
            << "workloadNode constructor but yaml entry doesn't have a workload entry";
        exit(EXIT_FAILURE);
    }
    auto yamlWorkload = ynode["workload"];
    myWorkload = unique_ptr<workload>(new workload(yamlWorkload));
}

// Execute the node
void workloadNode::execute(shared_ptr<threadState> myState) {
    myWorkload->uri = myState->myWorkload.uri;
    BOOST_LOG_TRIVIAL(debug) << "In workloadNode and executing";
    // set random seed based on current random seed.
    // should it be set in constructor? Is that safe?
    myWorkload->setRandomSeed(myState->rng());
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
