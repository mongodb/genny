#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"
#include "parse_util.hpp"
#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class set_variable : public operation {
public:
    set_variable(YAML::Node&);
    set_variable() = delete;
    virtual ~set_variable() = default;
    set_variable(const set_variable&) = default;
    set_variable(set_variable&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    string targetVariable;
    bool useVariable;
    string donorVariable;
    // move to optional
    bsoncxx::array::value myValue{bsoncxx::builder::stream::array()
                                  << 0 << bsoncxx::builder::stream::finalize};
    // Really should have a transform of some sort here, possibly built on a value
};
}
