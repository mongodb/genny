#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class create_index : public operation {
public:
    create_index(YAML::Node&);
    create_index() = delete;
    virtual ~create_index() = default;
    create_index(const create_index&) = default;
    create_index(create_index&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> keys{};
    // only one of these is really needed, but the view version is broken
    unique_ptr<document> options{};
    mongocxx::options::index indexOptions{};
};
}
