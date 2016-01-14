#include <random>
#include <thread>
#include <mongocxx/client.hpp>
#include <bsoncxx/types/value.hpp>
#include <unordered_map>
#include <atomic>
#include <bsoncxx/stdx/optional.hpp>

#pragma once
using namespace std;

namespace mwg {

class node;
class workload;

class threadState {
public:
    threadState(uint64_t seed,
                unordered_map<string, bsoncxx::array::value> tvars,
                unordered_map<string, bsoncxx::array::value>& wvars,
                workload& parentWorkload,
                string dbname,
                string collectionname,
                string uri = mongocxx::uri::k_default_uri)
        : conn(mongocxx::uri(uri)),
          rng(seed),
          tvariables(tvars),
          wvariables(wvars),
          myWorkload(parentWorkload),
          DBName(dbname),
          CollectionName(collectionname),
          stopped(false){};
    mongocxx::client conn;
    mt19937_64 rng;  // random number generator
    shared_ptr<node> currentNode;
    unordered_map<string, bsoncxx::array::value> tvariables;
    unordered_map<string, bsoncxx::array::value>& wvariables;
    bsoncxx::stdx::optional<bsoncxx::array::value> result;
    vector<shared_ptr<threadState>> childThreadStates;
    vector<shared_ptr<threadState>> backgroundThreadStates;
    vector<shared_ptr<thread>> childThreads;
    vector<shared_ptr<thread>> backgroundThreads;
    shared_ptr<threadState> parentThread;
    shared_ptr<thread> myThread;
    workload& myWorkload;
    string DBName;
    string CollectionName;
    std::atomic<bool> stopped;
};
}
