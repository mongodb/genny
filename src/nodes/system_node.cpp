#include "system_node.hpp"

#include <boost/log/trivial.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "parse_util.hpp"

namespace mwg {
SystemNode::SystemNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "system") {
        BOOST_LOG_TRIVIAL(fatal)
            << "SystemNode constructor but yaml entry doesn't have type == system";
        exit(EXIT_FAILURE);
    }
    if (!ynode["command"]) {
        BOOST_LOG_TRIVIAL(fatal) << "In system node but no command";
        exit(EXIT_FAILURE);
    }
    command = ynode["command"].Scalar();
    BOOST_LOG_TRIVIAL(debug) << "System node command is: " << command;
}

// Execute the node
void SystemNode::execute(shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(debug) << "SystemNode.execute. Executing: " << command;
    std::system(command.c_str());
    BOOST_LOG_TRIVIAL(debug) << "System node executed command";
}
}
