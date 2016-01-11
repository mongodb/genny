#include "delete_many.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace mwg {

delete_many::delete_many(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = delete_many
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Delete_Many constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in delete_many type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "delete_many") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Delete_Many constructor but yaml entry doesn't have type == delete_many";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseDeleteOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type delete_many";
}

// Execute the node
void delete_many::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto result = collection.delete_many(view, options);
    } catch (mongocxx::operation_exception e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in delete_many: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    BOOST_LOG_TRIVIAL(debug) << "delete_many.execute: delete_many is "
                             << bsoncxx::to_json(filter->view(mydoc, state));
}
}
