#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class replace_one : public operation {
public:
    replace_one(YAML::Node&);
    replace_one() = delete;
    virtual ~replace_one() = default;
    replace_one(const replace_one&) = default;
    replace_one(replace_one&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    unique_ptr<document> replacement{};
    mongocxx::options::update options{};
};
}
