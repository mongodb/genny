#include "yaml-cpp/yaml.h"

#include "node.hpp"
#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

    class opNode : public node {
        
    public: 
        
        opNode(YAML::Node &);
        opNode() = delete;
        virtual ~opNode() = default;
        opNode(const opNode&) = default;
        opNode(opNode&&) = default;
        // Execute the node
        virtual void execute(mongocxx::client &, mt19937_64 &) override;
        
    private:
        unique_ptr<operation> op;
    };


}
