#pragma once

#include "node.hpp"
#include "workload.hpp"

using namespace std;

namespace mwg {

class Spawn : public node {
public:
    Spawn(YAML::Node&);
    Spawn() = delete;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&,
                             vector<shared_ptr<node>>&) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;

private:
    vector<shared_ptr<node>> spawnNodes;
    vector<string> nodeNames;
};
}
