#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class run_command : public operation {
public:
    run_command(YAML::Node&);
    run_command() = delete;
    virtual ~run_command() = default;
    run_command(const run_command&) = default;
    run_command(run_command&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> command{};
    string collection_name;
};
}
