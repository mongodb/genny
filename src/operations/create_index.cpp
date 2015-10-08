#include "create_index.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

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
    else
        options = unique_ptr<document>(new document());
    keys = makeDoc(node["keys"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type create_index";
}

// Execute the node
void create_index::execute(mongocxx::client& conn, threadState& state) {
    BOOST_LOG_TRIVIAL(debug) << "Enter create_index execute";
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    bsoncxx::builder::stream::document myoptions{};
    auto view = keys->view(mydoc, state);
    auto opview = options->view(myoptions, state);
    try {
        auto result = collection.create_index(view, opview);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in create_index: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    BOOST_LOG_TRIVIAL(debug) << "create_index.execute: create_index is " << bsoncxx::to_json(view)
                             << " with options " << bsoncxx::to_json(opview);
}
}
