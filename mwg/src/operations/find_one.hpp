#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class find_one : public operation {
public:
    find_one(YAML::Node&);
    find_one() = delete;
    virtual ~find_one() = default;
    find_one(const find_one&) = default;
    find_one(find_one&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::find options{};
};
}
