#include "node.hpp"
#include "insert.hpp"
#include "query.hpp"
#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <unordered_map>
#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#pragma once
using namespace std;

namespace mwg {
    
    class workload : public node {
        
    public: 
        
        workload() {};
        workload(YAML::Node &nodes);
        virtual ~workload() = default;
        workload(const workload&) = default;
        workload(workload&&) = default;
        // Execute the workload
        virtual void execute(mongocxx::client &);
        
    private:
        unordered_map<string,shared_ptr<node>> nodes;
        
};
}

