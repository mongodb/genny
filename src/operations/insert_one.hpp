#include "yaml-cpp/yaml.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "document.hpp"
#include "operation.hpp"
#pragma once
using namespace std;

namespace mwg {

class insert_one : public operation {
public:
    insert_one(YAML::Node&);
    insert_one() = delete;
    virtual ~insert_one() = default;
    insert_one(const insert_one&) = default;
    insert_one(insert_one&&) = default;
    // Execute the node
    void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> document{};
    mongocxx::options::insert options{};
};
}
