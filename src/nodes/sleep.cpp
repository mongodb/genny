#include "sleep.hpp"

#include <boost/log/trivial.hpp>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "parse_util.hpp"

namespace mwg {
sleepNode::sleepNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "sleep") {
        BOOST_LOG_TRIVIAL(fatal)
            << "SleepNode constructor but yaml entry doesn't have type == sleep";
        exit(EXIT_FAILURE);
    }
    sleeptimeMs = IntOrValue(ynode["sleepMs"]);
}

// Execute the node
void sleepNode::execute(shared_ptr<threadState> myState) {
    auto this_sleep_time_ms = std::chrono::milliseconds(sleeptimeMs.getInt(*myState));
    BOOST_LOG_TRIVIAL(debug) << "sleepNode.execute. Sleeping for " << this_sleep_time_ms.count()
                             << " ms";
    std::this_thread::sleep_for(this_sleep_time_ms);
    BOOST_LOG_TRIVIAL(debug) << "Slept.";
}
}
