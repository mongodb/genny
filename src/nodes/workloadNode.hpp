#pragma once

#include <bsoncxx/stdx/optional.hpp>
#include <memory>

#include "node.hpp"
#include "value_generator.hpp"

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
    unique_ptr<ValueGenerator> dbName;
    unique_ptr<ValueGenerator> collectionName;
    unique_ptr<ValueGenerator> workloadName;
    unique_ptr<ValueGenerator> numThreads;
    unique_ptr<ValueGenerator> runLengthMs;
};
}
