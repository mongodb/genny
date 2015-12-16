#include "drop.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

drop::drop(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = drop
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Drop constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in drop type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "drop") {
        BOOST_LOG_TRIVIAL(fatal) << "Drop constructor but yaml entry doesn't have type == drop";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(debug) << "Added op of type drop";
}

// Execute the node
void drop::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    try {
        collection.drop();
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in drop collection: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    BOOST_LOG_TRIVIAL(debug) << "drop.execute";
}
}
