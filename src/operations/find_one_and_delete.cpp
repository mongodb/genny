#include "find_one_and_delete.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

find_one_and_delete::find_one_and_delete(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find_one_and_delete
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Find_One_And_Delete constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find_one_and_delete type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "find_one_and_delete") {
        BOOST_LOG_TRIVIAL(fatal) << "Find_One_And_Delete constructor but yaml entry doesn't have "
                                    "type == find_one_and_delete";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseFindOneAndDeleteOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type find_one_and_delete";
}

// Execute the node
void find_one_and_delete::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto value = collection.find_one_and_delete(view, options);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in find_one_and_delete: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
    BOOST_LOG_TRIVIAL(debug) << "find_one_and_delete.execute: find_one_and_delete is "
                             << bsoncxx::to_json(view);
}
}
