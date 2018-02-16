#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class update_one : public operation {
public:
    update_one(YAML::Node&);
    update_one() = delete;
    virtual ~update_one() = default;
    update_one(const update_one&) = default;
    update_one(update_one&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    unique_ptr<document> update{};
    mongocxx::options::update options{};
};
}
