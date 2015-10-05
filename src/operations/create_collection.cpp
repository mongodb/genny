#include "create_collection.hpp"
#include "parse_util.hpp"
#include "../documents/bsonDocument.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

create_collection::create_collection(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = create_collection
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Create_Collection constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in create_collection type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "create_collection") {
        BOOST_LOG_TRIVIAL(fatal) << "Create_Collection constructor but yaml entry doesn't have "
                                    "type == create_collection";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        options = makeDoc(node["options"]);
    else
        options = unique_ptr<document>{new bsonDocument()};
    collection_name = node["collection_name"].Scalar();
    BOOST_LOG_TRIVIAL(debug) << "Added op of type create_collection";
}

// Execute the node
void create_collection::execute(mongocxx::client& conn, threadState& state) {
    auto db = conn["testdb"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = options->view(mydoc, state);
    db.create_collection(collection_name, view);
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "create_collection.execute: create_collection with name "
                             << collection_name << " and options " << bsoncxx::to_json(view);
}
}
