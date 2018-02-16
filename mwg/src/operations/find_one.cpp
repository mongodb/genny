#include "find_one.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <stdlib.h>

#include "node.hpp"
#include "parse_util.hpp"

using bsoncxx::builder::concatenate;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_document;

namespace mwg {

find_one::find_one(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Find_One constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find_one type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "find_one") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Find_One constructor but yaml entry doesn't have type == find_one";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseFindOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type find_one";
}

// Execute the node
void find_one::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto value = collection.find_one(view, options);
        state.result = bsoncxx::builder::stream::array()
            << open_document << concatenate(value->view()) << close_document
            << bsoncxx::builder::stream::finalize;
    } catch (mongocxx::operation_exception e) {
        state.currentNode->recordException();
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in find_one: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    BOOST_LOG_TRIVIAL(debug) << "find_one.execute: find_one is " << bsoncxx::to_json(view);
}
}
