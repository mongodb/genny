#include <stdio.h>
#include <stdlib.h>
#include "sleep.hpp"
#include <random>
#include "parse_util.hpp"
#include <thread>
#include <boost/log/trivial.hpp>

namespace mwg {
sleepNode::sleepNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "sleep") {
        BOOST_LOG_TRIVIAL(fatal)
            << "SleepNode constructor but yaml entry doesn't have type == sleep";
        exit(EXIT_FAILURE);
    }
    sleeptime = std::chrono::milliseconds(ynode["sleep"].as<uint64_t>());
    BOOST_LOG_TRIVIAL(debug) << "In sleepNode constructor. Sleep time is " << sleeptime.count();
}

// Execute the node
void sleepNode::execute(shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(debug) << "sleepNode.execute. Sleeping for " << sleeptime.count() << " ms";
    std::this_thread::sleep_for(sleeptime);
    BOOST_LOG_TRIVIAL(debug) << "Slept.";
}
}
