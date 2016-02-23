#pragma once

#include "node.hpp"
#include "workload.hpp"

using namespace std;

namespace mwg {

class workload;
class ForN : public node {
public:
    ForN(YAML::Node&);
    ForN() = delete;
    virtual void execute(shared_ptr<threadState>) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;
    virtual bsoncxx::document::value getStats(bool withReset) override;
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&,
                             vector<shared_ptr<node>>&) override;

protected:
    shared_ptr<node> myNode;
    string myNodeName;
    uint64_t N;
};
}
