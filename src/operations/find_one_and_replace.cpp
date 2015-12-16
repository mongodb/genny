#include "find_one_and_replace.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

find_one_and_replace::find_one_and_replace(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find_one_and_replace
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Find_One_And_Replace constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find_one_and_replace type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "find_one_and_replace") {
        BOOST_LOG_TRIVIAL(fatal) << "Find_One_And_Replace constructor but yaml entry doesn't have "
                                    "type == find_one_and_replace";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseFindOneAndReplaceOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    replace = makeDoc(node["replace"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type find_one_and_replace";
}

// Execute the node
void find_one_and_replace::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    bsoncxx::builder::stream::document mydoc{};
    bsoncxx::builder::stream::document myreplace{};
    auto view = filter->view(mydoc, state);
    auto replaceview = replace->view(myreplace, state);
    try {
        auto value = collection.find_one_and_replace(view, replaceview, options);
        // need a way to exhaust the cursor
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in find_one_and_replace: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
    BOOST_LOG_TRIVIAL(debug) << "find_one_and_replace.execute: find_one_and_replace is "
                             << bsoncxx::to_json(view);
}
}
