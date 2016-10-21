#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mongocxx/client.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <yaml-cpp/yaml.h>

#include "stats.hpp"
#include "threadState.hpp"

using namespace std;

namespace mwg {

class node;

class workload;
class WorkloadExecutionState {
public:
    WorkloadExecutionState(workload* work) : myWorkload(work){};
    WorkloadExecutionState(const WorkloadExecutionState& other)
        : numParallelThreads(other.numParallelThreads),
          runLengthMs(other.runLengthMs),
          uri(other.uri),
          wvariables(other.wvariables),
          myWorkload(other.myWorkload),
          DBName(other.DBName),
          CollectionName(other.CollectionName){};
    mutex mut;
    uint64_t numParallelThreads = 1;
    uint64_t runLengthMs{0};
    string uri = mongocxx::uri::k_default_uri;
    unordered_map<string, bsoncxx::array::value> wvariables;  // workload variables
    workload* myWorkload;
    mt19937_64 rng;  // random number generator
    string DBName = "testDB";
    string CollectionName = "testCollection";

    // increase or decrease the number of threads
    void increaseThreads();
    void decreaseThreads();
    // Get the number of active threads in this workload
    uint64_t getActiveThreads();
    // Are there any active threads in this workload.
    bool anyThreadsActive();
    // Wait for all threads in this workload to be done
    void waitThreadsDone();

private:
    // Thread handling members
    mutex threadMut;
    condition_variable threadsDoneCV;
    uint64_t numActiveThreads = 0;
};

// Start a new thread with thread state and initial state
shared_ptr<thread> startThread(node*, shared_ptr<threadState>);

class workload {
public:
    workload() : baseWorkloadState(this), stopped(false){};
    workload(const YAML::Node& nodes);
    // Execute the workload
    virtual void execute(WorkloadExecutionState*);
    WorkloadExecutionState newWorkloadState() {
        return (move(WorkloadExecutionState(baseWorkloadState)));
    }
    // should stop be specific to a thread state -- probably not)
    void stop();
    // Going to need to protect access to this for thread
    // safety. Currently only done in one place

    // Generate a dot file for generating a graph.
    string generateDotGraph();

    void logStats();
    virtual bsoncxx::document::value getStats(bool withReset);  // bson object with stats
    void setRandomSeed(uint64_t seed, WorkloadExecutionState* state) {
        state->rng.seed(seed);
    };
    string name;

protected:
    WorkloadExecutionState baseWorkloadState;
    // This should be the owning copy.
    // Can't make a vector of unique_ptr with incomplete forward declared type node.
    vector<shared_ptr<node>> vectornodes;
    unordered_map<string, bsoncxx::array::value> tvariables;
    atomic<bool> stopped;
    Stats myStats;
};
}
