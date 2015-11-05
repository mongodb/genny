#include "distinct.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

distinct::distinct(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = distinct
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Distinct constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in distinct type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "distinct") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Distinct constructor but yaml entry doesn't have type == distinct";
        exit(EXIT_FAILURE);
    }
    if (!node["distinct_name"]) {
        BOOST_LOG_TRIVIAL(fatal) << "Distinct constructor but yaml entry doesn't have a name entry";
        exit(EXIT_FAILURE);
    }
    if (!node["filter"]) {
        BOOST_LOG_TRIVIAL(fatal)
            << "Distinct constructor but yaml entry doesn't have a filter entry";
        exit(EXIT_FAILURE);
    }
    name = node["distinct_name"].Scalar();
    if (node["options"])
        parseDistinctOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type distinct";
}

// Execute the node
void distinct::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto cursor = collection.distinct(name, view, options);
        // need a way to exhaust the cursor
        BOOST_LOG_TRIVIAL(debug) << "distinct.execute: distinct is " << bsoncxx::to_json(view);
        for (auto&& doc : cursor) {
            // std::cout << bsoncxx::to_json(doc) << std::endl;
            bsoncxx::to_json(doc);
        }
        BOOST_LOG_TRIVIAL(debug) << "After iterating results";
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in insert_many: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
}
}
