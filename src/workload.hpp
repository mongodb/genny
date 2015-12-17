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
    virtual ~workload() = default;
    workload(const workload&) = default;
    workload(workload&&) = default;
    // Execute the workload
    virtual void execute(mongocxx::client&);
    workload& operator=(const workload&) = default;
    workload& operator=(workload&&) = default;
    void stop();
    // Going to need to protect access to this for thread
    // safety. Currently only done in one place
    unordered_set<shared_ptr<threadState>> threads;

    // Generate a dot file for generating a graph.
    std::string generateDotGraph();

    std::mutex mut;
    string DBName = "testdb";
    string CollectionName = "testCollection";
    string uri = mongocxx::uri::k_default_uri;
    void logStats();
    virtual bsoncxx::document::value getStats(bool withReset);  // bson object with stats

private:
    vector<shared_ptr<node>> vectornodes;
    // needs to be generalized from int to a generic value
    unordered_map<string, bsoncxx::types::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::types::value> tvariables;  // thread variables
    mt19937_64 rng;                                           // random number generator
    string name;
    std::atomic<bool> stopped;
    uint64_t numParallelThreads{1};
    uint64_t runLength{0};
    stats myStats;
};
}
