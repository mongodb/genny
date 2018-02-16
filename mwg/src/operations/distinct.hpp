#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class distinct : public operation {
public:
    distinct(YAML::Node&);
    distinct() = delete;
    virtual ~distinct() = default;
    distinct(const distinct&) = default;
    distinct(distinct&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::distinct options{};
    string name;
};
}
