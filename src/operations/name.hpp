#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class name : public operation {
public:
    name(YAML::Node&);
    name() = delete;
    virtual ~name() = default;
    name(const name&) = default;
    name(name&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
};
}
