#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class delete_one : public operation {
public:
    delete_one(YAML::Node&);
    delete_one() = delete;
    virtual ~delete_one() = default;
    delete_one(const delete_one&) = default;
    delete_one(delete_one&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::delete_options options{};
};
}
