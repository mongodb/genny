#pragma once

#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <random>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

using namespace std;

namespace mwg {
    
    class node;
    class workload  {
        
    public: 
        
        workload() {};
        workload(YAML::Node &nodes);
        virtual ~workload() = default;
        workload(const workload&) = default;
        workload(workload&&) = default;
        // Execute the workload
        virtual void execute(mongocxx::client &);
        workload & operator= ( const workload & ) = default;
        workload & operator= ( workload && ) = default;

    private:
        // really don't want both of these long term, but they make
        // life easier for now.
        unordered_map<string,shared_ptr<node>> nodes;
        vector<shared_ptr<node>> vectornodes;
        mt19937_64 rng; // random number generator
        string name;

};
}

