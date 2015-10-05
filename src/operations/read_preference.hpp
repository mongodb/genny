#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class read_preference : public operation {
public:
    read_preference(YAML::Node&);
    read_preference() = delete;
    virtual ~read_preference() = default;
    read_preference(const read_preference&) = default;
    read_preference(read_preference&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    mongocxx::read_preference read_pref;
};
}
