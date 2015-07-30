#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class find : public operation {
public:
    find(YAML::Node&);
    find() = delete;
    virtual ~find() = default;
    find(const find&) = default;
    find(find&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, mt19937_64&) override;

private:
    bsoncxx::builder::stream::document filter{};
    mongocxx::options::find options{};
};
}
