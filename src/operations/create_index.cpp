#include "create_index.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

create_index::create_index(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = create_index
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Create_Index constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in create_index type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "create_index") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Create_Index constructor but yaml entry doesn't have type == create_index";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        options = makeDoc(node["options"]);
    keys = makeDoc(node["keys"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type create_index";
}

// Execute the node
void create_index::execute(mongocxx::client& conn, threadState& state) {
    BOOST_LOG_TRIVIAL(debug) << "Enter create_index execute";
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = keys->view(mydoc, state);
    BOOST_LOG_TRIVIAL(debug) << "View of keys";
    bsoncxx::document::view opview{};  // default blank options
                                       // if (options != nullptr) {
                                       //     bsoncxx::builder::stream::document myoptions{};
                                       //     opview = options->view(myoptions, state);
                                       //     BOOST_LOG_TRIVIAL(debug) << "View of options";
                                       //     auto result = collection.create_index(view, opview);
                                       // } else
    auto result = collection.create_index(view);
    BOOST_LOG_TRIVIAL(debug) << "create_index.execute: create_index is " << bsoncxx::to_json(view);
}
}
