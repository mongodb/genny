#include "find.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

find::find(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Find constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "find") {
        BOOST_LOG_TRIVIAL(fatal) << "Find constructor but yaml entry doesn't have type == find";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseFindOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type find";
}

// Execute the node
void find::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto cursor = collection.find(view, options);
        // need a way to exhaust the cursor
        BOOST_LOG_TRIVIAL(debug) << "find.execute: find is " << bsoncxx::to_json(view);
        for (auto&& doc : cursor) {
            // std::cout << bsoncxx::to_json(doc) << std::endl;
            doc.length();
        }
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in find: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }

    BOOST_LOG_TRIVIAL(debug) << "After iterating results";
}
}
