#include "node.hpp"
#include "yaml-cpp/yaml.h"
#include <bsoncxx/stdx/optional.hpp>
#include <string>
#include <unordered_map>
#pragma once

using namespace std;

namespace mwg {

enum class comparison { EQUALS, GREATER_THAN, LESS_THAN, GREATER_THAN_EQUAL, LESS_THAN_EQUAL };

class ifNode : public node {
public:
    ifNode(YAML::Node&);
    ifNode() = delete;
    virtual ~ifNode() = default;
    ifNode(const ifNode&) = default;
    ifNode(ifNode&&) = default;
    // Execute the node
    virtual void executeNode(shared_ptr<threadState>) override;
    virtual void setNextNode(unordered_map<string, node*>&, vector<shared_ptr<node>>&) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;

private:
    // possible next states with probabilities
    string ifNodeName, elseNodeName;
    node *iffNode, *elseNode;
    bsoncxx::stdx::optional<bsoncxx::array::value> compareValue;
    comparison comparisonTest;
    string comparisonVariable;
};
}
