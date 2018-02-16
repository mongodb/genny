#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class find_one_and_replace : public operation {
public:
    find_one_and_replace(YAML::Node&);
    find_one_and_replace() = delete;
    virtual ~find_one_and_replace() = default;
    find_one_and_replace(const find_one_and_replace&) = default;
    find_one_and_replace(find_one_and_replace&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    unique_ptr<document> replace{};
    mongocxx::options::find_one_and_replace options{};
};
}
