#include "document.hpp"
#include "bsonDocument.hpp"

namespace mwg {

// parse a YAML Node and make a document of the correct type
unique_ptr<mdocument> makeDoc(YAML::Node& node) {
    return unique_ptr<mdocument> { new bsonDocument(node)};
};
}
