#pragma once

#include "node.hpp"
#include "workload.hpp"

using namespace std;

namespace mwg {

class workload;
class join : public node {
public:
    join(YAML::Node&);
    join() = delete;
    virtual ~join() = default;
    join(const join&) = default;
    join(join&&) = default;
    // Execute the node
    virtual void executeNode(shared_ptr<threadState>) override;

private:
    vector<shared_ptr<node>> vectornodes;
    vector<string> nodeNames;
    string joinName;
};
}
