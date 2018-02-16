#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include "yaml-cpp/yaml.h"

#include "document.hpp"

namespace mwg {
class bsonDocument : public document {
public:
    bsonDocument();
    bsonDocument(YAML::Node);

    void setDoc(bsoncxx::document::value value) {
        doc = value;
    }
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         threadState&) override;

private:
    bsoncxx::stdx::optional<bsoncxx::document::value> doc;
};
}
