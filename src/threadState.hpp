#include <atomic>
#include <bsoncxx/stdx/optional.hpp>
#include <bsoncxx/types/value.hpp>
#include <mongocxx/client.hpp>
#include <random>
#include <thread>
#include <unordered_map>

#pragma once
using namespace std;

namespace mwg {

class node;
class workload;
class WorkloadExecutionState;

class threadState {
public:
    threadState(uint64_t seed,
                unordered_map<string, bsoncxx::array::value> tvars,
                unordered_map<string, bsoncxx::array::value>& wvars,
                WorkloadExecutionState* parentWorkload,
                string dbname,
                string collectionname,
                string uri = mongocxx::uri::k_default_uri)
        : conn(mongocxx::uri(uri)),
          rng(seed),
          tvariables(tvars),
          wvariables(wvars),
          workloadState(parentWorkload),
          DBName(dbname),
          CollectionName(collectionname),
          stopped(false){};
    mongocxx::client conn;
    mt19937_64 rng;  // random number generator
    node* currentNode;
    unordered_map<string, bsoncxx::array::value> tvariables;
    unordered_map<string, bsoncxx::array::value>& wvariables;
    bsoncxx::stdx::optional<bsoncxx::array::value> result;
    // These should be owned here, rather than being shared.
    // The workload has to own the threadstats and threads for the top level threads
    vector<shared_ptr<threadState>> backgroundThreadStates;
    vector<shared_ptr<thread>> childThreads;
    vector<shared_ptr<thread>> backgroundThreads;
    WorkloadExecutionState* workloadState;
    string DBName;
    string CollectionName;
    std::atomic<bool> stopped;
};
}
