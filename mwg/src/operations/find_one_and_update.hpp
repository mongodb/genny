#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class find_one_and_update : public operation {
public:
    find_one_and_update(YAML::Node&);
    find_one_and_update() = delete;
    virtual ~find_one_and_update() = default;
    find_one_and_update(const find_one_and_update&) = default;
    find_one_and_update(find_one_and_update&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    unique_ptr<document> update{};
    mongocxx::options::find_one_and_update options{};
};
}
