#include "bsonDocument.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

using namespace std;


namespace mwg {

bsonDocument::bsonDocument(YAML::Node node) {
    if (!node) {
        BOOST_LOG_TRIVIAL(error) << "bsonDocument constructor using empty document";
    } else if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in bsonDocument constructor";
        exit(EXIT_FAILURE);
    } else {
        BOOST_LOG_TRIVIAL(trace) << "In bsonDocument constructor";
        parseMap(doc, node);
        BOOST_LOG_TRIVIAL(trace) << "Parsed map in bsonDocument constructor";
    }
}

bsoncxx::document::view bsonDocument::view(bsoncxx::builder::stream::document&, threadState&) {
    return doc.view();
}
}
