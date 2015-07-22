#include "node.hpp"
#include "insert.hpp"
#include "query.hpp"
#include "random_choice.hpp"
#include "sleep.hpp"
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>
#include <memory>
#include <vector>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#pragma once
using namespace std;

namespace mwg {
    
    class workload  {
        
    public: 
        
        workload() {};
        workload(YAML::Node &nodes);
        virtual ~workload() = default;
        workload(const workload&) = default;
        workload(workload&&) = default;
        // Execute the workload
        virtual void execute(mongocxx::client &);
        
        // really don't want both of these long term, but they make
        // life easier for now.
        unordered_map<string,shared_ptr<node>> nodes;
        vector<shared_ptr<node>> vectornodes;
};
}

