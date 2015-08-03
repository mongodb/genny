#include "document.hpp"
#include "bsonDocument.hpp"
#include <memory>

#pragma once

using namespace std;

namespace mwg {
class overrideDocument : public mdocument {
public:
    overrideDocument() {};
    overrideDocument(YAML::Node&);
    virtual ~overrideDocument() = default;
    overrideDocument(const overrideDocument&) = default;
    overrideDocument(overrideDocument&&) = default;

    virtual bsoncxx::document::view get();

private:
    // The document to override
    bsonDocument doc;
    // The list of things to override. 
    // These are strings for now. Need to be generalized to a value type. 
    //    vector<string, string> overide;
};
}
