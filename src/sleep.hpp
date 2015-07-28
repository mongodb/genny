#include "yaml-cpp/yaml.h"

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
        void executeNode(mongocxx::client &, mt19937_64 &) override;
        
    private:
        // possible next states with probabilities
        uint64_t sleeptime;
};
}
