#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>

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
    void execute(mongocxx::client &, mt19937_64 &) override;

private:
    bsoncxx::builder::stream::document querydoc{};
};
}
