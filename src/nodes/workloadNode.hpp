#pragma once

#include "node.hpp"
#include <memory>
#include <bsoncxx/stdx/optional.hpp>

using namespace std;
using bsoncxx::stdx::optional;

namespace mwg {
class workload;
class workloadNode : public node {
public:
    workloadNode(YAML::Node&);
    workloadNode() = delete;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;
    virtual std::pair<std::string, std::string> generateDotGraph() override;

    virtual void logStats() override;  // print out the stats
    virtual bsoncxx::document::value getStats(bool withReset) override;
    virtual void stop() override;

protected:
    unique_ptr<workload> myWorkload;
    optional<YAML::Node> dbName;
    optional<YAML::Node> collectionName;
    optional<YAML::Node> workloadName;
    optional<YAML::Node> numThreads;
    optional<YAML::Node> runLength;
};
}
