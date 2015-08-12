#include "yaml-cpp/yaml.h"
#include "document.hpp"
#include <bsoncxx/builder/stream/document.hpp>

#pragma once

namespace mwg {
class bsonDocument : public document {
public:
    bsonDocument(){};
    bsonDocument(YAML::Node);
    virtual ~bsonDocument() = default;
    bsonDocument(const bsonDocument&) = default;
    bsonDocument(bsonDocument&&) = default;
    bsonDocument& operator=(const bsonDocument&) = default;
    bsonDocument& operator=(bsonDocument&&) = default;

    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         threadState&) override;

private:
    bsoncxx::builder::stream::document doc{};
};
}
