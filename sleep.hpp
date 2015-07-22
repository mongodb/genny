#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "parse_util.hpp"
#include <unordered_map>
#include <time.h>

#include "node.hpp"
#pragma once

using namespace std;

namespace mwg {
    
    class sleepNode : public node {
        
    public: 
        
        sleepNode(YAML::Node &);
        sleepNode() = delete;
        virtual ~sleepNode() = default;
        sleepNode(const sleepNode&) = default;
        sleepNode(sleepNode&&) = default;
        // Execute the node
        void executeNode(mongocxx::client &) override;
        
    private:
        // possible next states with probabilities
        long long sleeptime;
};
}
