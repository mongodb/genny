#include "document.hpp"
#include "bsonDocument.hpp"

namespace mwg {

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(YAML::Node node) {
    return unique_ptr<document>{new bsonDocument(node)};
};
}
