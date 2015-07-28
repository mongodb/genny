#include "yaml-cpp/yaml.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "operation.hpp"
#pragma once
using namespace std;

namespace mwg {

    class insert_many : public operation {

public: 

    insert_many(YAML::Node &);
    insert_many() = delete;
    virtual ~insert_many() = default;
    insert_many(const insert_many&) = default;
    insert_many(insert_many&&) = default;
    // Execute the node
    void execute(mongocxx::client &, mt19937_64 &) override;

private:
        vector<bsoncxx::builder::stream::document> collection;
    mongocxx::options::insert options{};    
};
}
