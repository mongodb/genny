#include "list_indexes.hpp"
#include "parse_util.hpp"
#include "node.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace mwg {

list_indexes::list_indexes(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = list_indexes
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "List_Indexes constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in list_indexes type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "list_indexes") {
        BOOST_LOG_TRIVIAL(fatal)
            << "List_Indexes constructor but yaml entry doesn't have type == list_indexes";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(debug) << "Added op of type list_indexes";
}

// Execute the node
void list_indexes::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    try {
        auto cursor = collection.list_indexes();
        for (auto&& doc : cursor) {
            doc.length();
        }
    } catch (mongocxx::operation_exception e) {
        state.currentNode->recordException();
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in list_indexes: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    BOOST_LOG_TRIVIAL(debug) << "list_indexes.execute";
}
}
