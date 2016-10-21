#include "yaml-cpp/yaml.h"
#include <string>
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
    virtual void setNextNode(unordered_map<string, node*>&, vector<shared_ptr<node>>&) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;

private:
    // possible next states with probabilities
    vector<pair<string, double>> vectornodestring;
    vector<pair<node*, double>> vectornodes;
    double total;
};
}
