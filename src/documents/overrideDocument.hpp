#include "document.hpp"
#include "bsonDocument.hpp"
#include <unordered_map>

#pragma once

using namespace std;

namespace mwg {
class overrideDocument : public document {
public:
    overrideDocument(){};
    overrideDocument(YAML::Node&);
    virtual ~overrideDocument() = default;
    overrideDocument(const overrideDocument&) = default;
    overrideDocument(overrideDocument&&) = default;
    overrideDocument& operator=(const overrideDocument&) = default;
    overrideDocument& operator=(overrideDocument&&) = default;

    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         threadState&) override;

private:
    // apply the overides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&,
                            bsoncxx::document::view,
                            string,
                            threadState&);
    //    void applyOverrideLevel(bsoncxx::builder::stream::document &,
    //    bsoncxx::document::view::iterator begin, bsoncxx::document::view::iterator end, string);
    // The document to override
    bsonDocument doc;
    // The list of things to override.
    // These are strings for now. Need to be generalized to a value type.
    unordered_map<string, YAML::Node> override;
};
}
