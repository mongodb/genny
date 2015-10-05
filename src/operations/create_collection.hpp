#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class create_collection : public operation {
public:
    create_collection(YAML::Node&);
    create_collection() = delete;
    virtual ~create_collection() = default;
    create_collection(const create_collection&) = default;
    create_collection(create_collection&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> options{};
    string collection_name;
};
}
