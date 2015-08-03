#include "bsonDocument.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>

using namespace std;


namespace mwg {

bsonDocument::bsonDocument(YAML::Node& node) {
    if (!node) {
        cerr << "bsonDocument constructor and !node" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        cerr << "Not map in bsonDocument constructor" << endl;
        exit(EXIT_FAILURE);
    }
    parseMap(doc, node);
}

bsoncxx::document::view bsonDocument::get() {
    return doc.view();
}
}
