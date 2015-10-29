#pragma once

#include "node.hpp"
#include "workload.hpp"

using namespace std;

namespace mwg {

class workload;
class workloadNode : public node {
public:
    workloadNode(YAML::Node&);
    workloadNode() = delete;
    virtual ~workloadNode() = default;
    workloadNode(const workloadNode&) = default;
    workloadNode(workloadNode&&) = default;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;

private:
    workload myWorkload;
};
}
