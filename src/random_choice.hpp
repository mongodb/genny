#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>

#include "node.hpp"
#pragma once

using namespace std;

namespace mwg {

class random_choice : public node {
public:
    random_choice(YAML::Node&);
    random_choice() = delete;
    virtual ~random_choice() = default;
    random_choice(const random_choice&) = default;
    random_choice(random_choice&&) = default;
    // Execute the node
    virtual void executeNode(shared_ptr<threadState>) override;
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&) override;

private:
    // possible next states with probabilities
    vector<pair<string, double>> vectornodestring;
    vector<pair<shared_ptr<node>, double>> vectornodes;
    double total;
};
}
