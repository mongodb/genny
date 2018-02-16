#pragma once

#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/stdx/optional.hpp>

#include "document.hpp"
#include "parse_util.hpp"
#include "operation.hpp"
#include "value_generator.hpp"

using bsoncxx::stdx::optional;

namespace mwg {

class set_variable : public operation {
public:
    set_variable(YAML::Node&);
    set_variable() = delete;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    string targetVariable;
    unique_ptr<ValueGenerator> valueGenerator;
};
}
