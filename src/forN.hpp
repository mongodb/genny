#pragma once

#include "node.hpp"
#include "workload.hpp"


using namespace std;

namespace mwg {

    class workload;
    class forN : public node {
    public : 
        forN(YAML::Node &);
        forN() = delete;
        virtual ~forN() = default;
        forN(const forN&) = default;
        forN(forN&&) = default;
        // Execute the node
        void execute(mongocxx::client &, mt19937_64 &) override;
        
    private :
        workload myWorkload;
        uint64_t N;

    };



}
