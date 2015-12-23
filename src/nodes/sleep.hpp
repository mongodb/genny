#include "yaml-cpp/yaml.h"
#include <chrono>
#include "node.hpp"
#pragma once

using namespace std;

namespace mwg {

class sleepNode : public node {
public:
    sleepNode(YAML::Node&);
    sleepNode() = delete;
    virtual ~sleepNode() = default;
    sleepNode(const sleepNode&) = default;
    sleepNode(sleepNode&&) = default;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;

private:
    // possible next states with probabilities
    std::chrono::milliseconds sleeptime;
};
}
