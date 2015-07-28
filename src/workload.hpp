#pragma once

#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <random>
#include <unordered_set>
#include <mongocxx/client.hpp>

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

    class workload  {
        
    public: 
        
        workload() : stopped (false) {};
        workload(YAML::Node &nodes);
        virtual ~workload() = default;
        workload(const workload&) = default;
        workload(workload&&) = default;
        // Execute the workload
        virtual void execute(mongocxx::client &);
        workload & operator= ( const workload & ) = default;
        workload & operator= ( workload && ) = default;
        void stop ();
        unordered_set<unique_ptr<threadState>> threads;


    private:
        vector<shared_ptr<node>> vectornodes;
        mt19937_64 rng; // random number generator
        string name;
        bool stopped;
        uint64_t numParallelThreads {1};
        uint64_t runLength {0};
};

}

