#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class update_many : public operation {
public:
    update_many(YAML::Node&);
    update_many() = delete;
    virtual ~update_many() = default;
    update_many(const update_many&) = default;
    update_many(update_many&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    unique_ptr<document> update{};
    mongocxx::options::update options{};
};
}
