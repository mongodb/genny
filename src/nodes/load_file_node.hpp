#pragma once

#include <boost/filesystem.hpp>
#include "yaml-cpp/yaml.h"

#include "node.hpp"

using namespace std;

namespace mwg {

class LoadFileNode : public node {
public:
    LoadFileNode(YAML::Node&);
    LoadFileNode() = delete;
    // Execute the node
    virtual void execute(shared_ptr<threadState>) override;

private:
    // The name of the file to load
    boost::filesystem::path filePath;
    mongocxx::options::insert options{};
};
}
