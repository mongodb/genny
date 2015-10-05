#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class write_concern : public operation {
public:
    write_concern(YAML::Node&);
    write_concern() = delete;
    virtual ~write_concern() = default;
    write_concern(const write_concern&) = default;
    write_concern(write_concern&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    mongocxx::write_concern write_conc;
};
}
