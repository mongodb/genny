#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include "yaml-cpp/yaml.h"
#include "../threadState.hpp"

#pragma once
using namespace std;
namespace mwg {

class document {
public:
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&, threadState&) = 0;
};

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(YAML::Node);
}
