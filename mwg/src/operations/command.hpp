#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class command : public operation {
public:
    command(YAML::Node&);
    command() = delete;
    virtual ~command() = default;
    command(const command&) = default;
    command(command&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> myCommand{};
    string collection_name;
};
}
