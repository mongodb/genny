#include <random>
#include <mongocxx/client.hpp>
#include <memory>

#pragma once
using namespace std;

namespace mwg {
    
    class node;
  
    class threadState {
    public: 
        threadState() {};
        threadState(uint64_t seed) : rng(seed) {}; // I'd like to pass more state here
        mongocxx::client conn {};
        mt19937_64 rng; // random number generator
        shared_ptr<node> currentNode;
    };
}
