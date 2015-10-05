#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class drop : public operation {
public:
    drop(YAML::Node&);
    drop() = delete;
    virtual ~drop() = default;
    drop(const drop&) = default;
    drop(drop&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
};
}
