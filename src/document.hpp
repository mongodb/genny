#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include "yaml-cpp/yaml.h"

#pragma once

namespace mwg {

class mdocument {
public:
    virtual bsoncxx::document::view get()=0;
};
}

//unique_ptr<mdocument> makeDoc(YAML::Node&) {return nullptr};
