#pragma once

#include <yaml-cpp/yaml.h>

#include "node.hpp"

using namespace std;

namespace mwg {

class SystemNode : public node {
public:
    SystemNode(YAML::Node&);
    SystemNode() = delete;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;

private:
    // command to execute
    string command;
};
}
