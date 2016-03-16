#pragma once

#include <memory>
#include <unordered_map>

#include "bsonDocument.hpp"
#include "document.hpp"
#include "value_generator.hpp"

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
    // The document to override
    bsonDocument doc;
    unordered_map<string, unique_ptr<ValueGenerator>> override;
};
}
