#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class delete_many : public operation {
public:
    delete_many(YAML::Node&);
    delete_many() = delete;
    virtual ~delete_many() = default;
    delete_many(const delete_many&) = default;
    delete_many(delete_many&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::delete_options options{};
};
}
