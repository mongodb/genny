#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include "parse_util.hpp"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "node.hpp"
#pragma once
using namespace std;

namespace mwg {

    class insert : public node {

public: 

    insert(YAML::Node &);
    insert() = delete;
    virtual ~insert() = default;
    insert(const insert&) = default;
    insert(insert&&) = default;
    // Execute the node
    void execute(mongocxx::client &, mt19937_64 &) override;

private:
    bsoncxx::builder::stream::document insertdoc{};
};
}
