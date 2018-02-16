#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include "document.hpp"

#include "operation.hpp"
#pragma once

using namespace std;

namespace mwg {

class count : public operation {
public:
    count(YAML::Node&);
    count() = delete;
    virtual ~count() = default;
    count(const count&) = default;
    count(count&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) override;

private:
    unique_ptr<document> filter{};
    mongocxx::options::count options{};
    int64_t assertEquals = -1;
};
}
