#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include "yaml-cpp/yaml.h"
#include "../threadState.hpp"

#pragma once
using namespace std;
namespace mwg {

class document {
public:
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc, threadState&) {
        return doc.view();
    };
};

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(YAML::Node);
}
