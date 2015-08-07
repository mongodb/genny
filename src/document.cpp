#include "document.hpp"
#include "bsonDocument.hpp"
#include "overrideDocument.hpp"

namespace mwg {

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(YAML::Node node) {
    if (!node["type"] or node["type"].Scalar() == "bson") {
        return unique_ptr<document>{new bsonDocument(node)};
    } else if (node["type"].Scalar() == "override") {
        return unique_ptr<document>{new overrideDocument(node)};
    } else {
        cerr << "in makeDoc and type exists, and isn't bson or override" << endl;
        exit(EXIT_FAILURE);
    }
};
}
