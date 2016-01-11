#include "write_concern.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace mwg {

write_concern::write_concern(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = write_concern
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Write_Concern constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in write_concern type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "write_concern") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Write_Concern constructor but yaml entry doesn't have type == write_concern";
        exit(EXIT_FAILURE);
    }
    write_conc = parseWriteConcern(node["write_concern"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type write_concern";
}

// Execute the node
void write_concern::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    try {
        collection.write_concern(write_conc);
    } catch (mongocxx::operation_exception e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in write_concern: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "write_concern.execute";
}
}
