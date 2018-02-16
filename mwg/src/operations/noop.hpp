#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class noop : public operation {
public:
    noop(YAML::Node&);
    noop() = delete;
    virtual ~noop() = default;
    noop(const noop&) = default;
    noop(noop&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
};
}
