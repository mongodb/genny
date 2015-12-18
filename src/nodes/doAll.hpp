#pragma once

#include "node.hpp"
#include "workload.hpp"

using namespace std;

namespace mwg {

class workload;
class doAll : public node {
public:
    doAll(YAML::Node&);
    doAll() = delete;
    virtual ~doAll() = default;
    doAll(const doAll&) = default;
    doAll(doAll&&) = default;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&,
                             vector<shared_ptr<node>>&) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;

private:
    vector<shared_ptr<node>> vectornodes;
    vector<shared_ptr<node>> vectorbackground;
    vector<string> nodeNames;
    vector<string> backgroundNodeNames;
    string joinName;
};
}
