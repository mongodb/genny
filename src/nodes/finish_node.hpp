#include "node.hpp"

#pragma once
using namespace std;

namespace mwg {

class finishNode : public node {
public:
    finishNode() {
        name = "Finish";
    };
    finishNode(YAML::Node&) {
        name = "Finish";
    };
    virtual ~finishNode() = default;
    finishNode(const finishNode&) = default;
    finishNode(finishNode&&) = default;
    // Execute the finishNode
    virtual void executeNode(shared_ptr<threadState>);
    const string getName() {
        return "Finish";
    };

    // The finish node never has a next pointer
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&){};
};
}
