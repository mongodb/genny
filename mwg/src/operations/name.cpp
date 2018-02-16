
#include "name.hpp"
#include "parse_util.hpp"
#include "node.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace mwg {

name::name(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = name
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Name constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in name type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "name") {
        BOOST_LOG_TRIVIAL(fatal) << "Name constructor but yaml entry doesn't have type == name";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(debug) << "Added op of type name";
}

// Execute the node
void name::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    try {
        auto name = collection.name();
        BOOST_LOG_TRIVIAL(debug) << "name.execute: name is " << name;
    } catch (mongocxx::operation_exception e) {
        state.currentNode->recordException();
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in name: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
}
}
