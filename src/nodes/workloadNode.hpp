#pragma once

#include "node.hpp"
#include <memory>

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
    virtual std::pair<std::string, std::string> generateDotGraph() override;

    virtual void logStats() override;  // print out the stats
    virtual bsoncxx::document::value getStats(bool withReset) override;
    virtual void stop() override;


private:
    unique_ptr<workload> myWorkload;
};
}
