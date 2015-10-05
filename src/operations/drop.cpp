#include "drop.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

drop::drop(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = drop
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Drop constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in drop type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "drop") {
        BOOST_LOG_TRIVIAL(fatal) << "Drop constructor but yaml entry doesn't have type == drop";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(debug) << "Added op of type drop";
}

// Execute the node
void drop::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    collection.drop();
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "drop.execute";
}
}
