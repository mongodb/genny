#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include "yaml-cpp/yaml.h"

#pragma once
using namespace std;
namespace mwg {

class document {
public:
    virtual bsoncxx::document::view view() = 0;
};

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(YAML::Node);
}
