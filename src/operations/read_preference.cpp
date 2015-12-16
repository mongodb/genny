#include "read_preference.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

read_preference::read_preference(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = read_preference
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Read_Preference constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in read_preference type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "read_preference") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Read_Preference constructor but yaml entry doesn't have type == read_preference";
        exit(EXIT_FAILURE);
    }
    read_pref = parseReadPreference(node["read_preference"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type read_preference";
}

// Execute the node
void read_preference::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    try {
        collection.read_preference(read_pref);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in read_preference: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "read_preference.execute";
}
}
