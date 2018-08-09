#include <gennylib/generators.hpp>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <random>
#include <stdlib.h>

namespace genny {

bsonDocument::bsonDocument() {
    doc = bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize;
}

bsonDocument::bsonDocument(const YAML::Node node) {
    if (!node) {
        BOOST_LOG_TRIVIAL(info) << "bsonDocument constructor using empty document";
    } else if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in bsonDocument constructor";
        exit(EXIT_FAILURE);
    } else {
        BOOST_LOG_TRIVIAL(trace) << "In bsonDocument constructor";
        doc = parseMap(node);
        BOOST_LOG_TRIVIAL(trace) << "Parsed map in bsonDocument constructor";
    }
}

bsoncxx::document::view bsonDocument::view(bsoncxx::builder::stream::document&, std::mt19937_64&) {
    return doc->view();
}

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(const YAML::Node node) {
    if (!node) {  // empty document should be bsonDocument
        return unique_ptr<document>{new bsonDocument(node)};
    } else  // if (!node["type"] or node["type"].Scalar() == "bson") {
        return unique_ptr<document>{new bsonDocument(node)};
    // } else if (!node["type"] or node["type"].Scalar() == "templating") {
    //   return unique_ptr<document>{new templateDocument(node)};
    // } else if (node["type"].Scalar() == "bson") {
    //   return unique_ptr<document>{new bsonDocument(node)};
    // } else if (node["type"].Scalar() == "override") {
    //   return unique_ptr<document>{new overrideDocument(node)};
    // } else if (node["type"].Scalar() == "append") {
    //   return unique_ptr<document>{new AppendDocument(node)};
    // } else {
    //   BOOST_LOG_TRIVIAL(fatal)
    //       << "in makeDoc and type exists, and isn't bson or override";
    //   exit(EXIT_FAILURE);
    // }
};
}  // namespace genny
