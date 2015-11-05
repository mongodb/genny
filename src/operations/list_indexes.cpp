#include "list_indexes.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

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
    auto collection = conn["testdb"]["testCollection"];
    try {
        auto cursor = collection.list_indexes();
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in list_indexes: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "list_indexes.execute";
}
}
