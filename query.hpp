#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include "parse_util.hpp"

#include "node.hpp"
#pragma once

using namespace std;

namespace mwg {

    class query : public node {

public: 

    query(YAML::Node &);
    query() = delete;
    virtual ~query() = default;
    query(const query&) = default;
    query(query&&) = default;
    // Execute the node
    void execute(mongocxx::client &);
    const string getName() {return name;};

private:
    bsoncxx::builder::stream::document querydoc{};
    string name;
};
}
