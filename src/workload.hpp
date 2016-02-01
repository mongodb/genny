#pragma once

#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <mongocxx/client.hpp>
#include "threadState.hpp"
#include <mutex>
#include "stats.hpp"

using namespace std;

namespace mwg {

class node;

class workload {
public:
    workload() : stopped(false){};
    workload(YAML::Node& nodes);
    // Execute the workload
    virtual void execute();
    void stop();
    // Going to need to protect access to this for thread
    // safety. Currently only done in one place
    unordered_set<shared_ptr<threadState>> threads;

    // Generate a dot file for generating a graph.
    std::string generateDotGraph();

    std::mutex mut;
    int64_t numParallelThreads;
    uint64_t runLength{0};
    string name;
    string uri = mongocxx::uri::k_default_uri;
    void logStats();
    virtual bsoncxx::document::value getStats(bool withReset);  // bson object with stats
    void setRandomSeed(uint64_t seed) {
        myState->rng.seed(seed);
    };
    threadState& getState() {
        return *myState;
    }

protected:
    vector<shared_ptr<node>> vectornodes;
    unique_ptr<threadState> myState;
    unordered_map<string, bsoncxx::array::value> wvariables;  // workload variables
    std::atomic<bool> stopped;
    stats myStats;
};
}
