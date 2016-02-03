#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/stdx/optional.hpp>

#include "document.hpp"
#include "parse_util.hpp"
#include "operation.hpp"
#pragma once

using bsoncxx::stdx::optional;

namespace mwg {

class set_variable : public operation {
public:
    set_variable(YAML::Node&);
    set_variable() = delete;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    enum class SetType { VALUE, OPERATION };
    string targetVariable;
    SetType setType;
    // move to optional
    bsoncxx::array::value myValue{bsoncxx::builder::stream::array()
                                  << 0 << bsoncxx::builder::stream::finalize};
    // Really should have a transform of some sort here, possibly built on a value
    optional<YAML::Node> operationNode;
};
}
