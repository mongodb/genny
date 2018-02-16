#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class find_one_and_delete : public operation {
public:
    find_one_and_delete(YAML::Node&);
    find_one_and_delete() = delete;
    virtual ~find_one_and_delete() = default;
    find_one_and_delete(const find_one_and_delete&) = default;
    find_one_and_delete(find_one_and_delete&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::find_one_and_delete options{};
};
}
