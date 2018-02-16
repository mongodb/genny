#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class list_indexes : public operation {
public:
    list_indexes(YAML::Node&);
    list_indexes() = delete;
    virtual ~list_indexes() = default;
    list_indexes(const list_indexes&) = default;
    list_indexes(list_indexes&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
};
}
