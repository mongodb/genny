#include "bsonDocument.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>

using namespace std;


namespace mwg {

bsonDocument::bsonDocument(YAML::Node node) {
    if (!node) {
        cerr << "bsonDocument constructor and !node" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        cerr << "Not map in bsonDocument constructor" << endl;
        exit(EXIT_FAILURE);
    }
    // cout << "In bsonDocument constructor" << endl;
    parseMap(doc, node);
    // cout << "Parsed map in bsonDocument constructor" << endl;
}

bsoncxx::document::view bsonDocument::view(bsoncxx::builder::stream::document&) {
    return doc.view();
}
}
