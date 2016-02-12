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
    sleeptimeMs = std::chrono::milliseconds(ynode["sleepMs"].as<uint64_t>());
    BOOST_LOG_TRIVIAL(debug) << "In sleepNode constructor. Sleep time is " << sleeptimeMs.count();
}

// Execute the node
void sleepNode::execute(shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(debug) << "sleepNode.execute. Sleeping for " << sleeptimeMs.count() << " ms";
    std::this_thread::sleep_for(sleeptimeMs);
    BOOST_LOG_TRIVIAL(debug) << "Slept.";
}
}
