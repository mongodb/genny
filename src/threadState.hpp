#include <random>
#include <thread>
#include <mongocxx/client.hpp>
#include <memory>
#include <unordered_map>
#include <atomic>

#pragma once
using namespace std;

namespace mwg {

class node;

class threadState {
public:
    threadState(uint64_t seed,
                unordered_map<string, int64_t> tvars,
                unordered_map<string, atomic_int_least64_t>& wvars)
        : rng(seed), tvariables(tvars), wvariables(wvars){};
    mongocxx::client conn{};
    mt19937_64 rng;  // random number generator
    shared_ptr<node> currentNode;
    unordered_map<string, int64_t> tvariables;
    unordered_map<string, atomic_int_least64_t>& wvariables;  // FIXME: Not threadsafe
    vector<shared_ptr<threadState>> childThreadStates;
    vector<shared_ptr<thread>> childThreads;
    shared_ptr<threadState> parentThread;
    shared_ptr<thread> myThread;
};
}
