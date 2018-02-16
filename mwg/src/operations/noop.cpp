#include "noop.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace mwg {

noop::noop(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = noop
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Noop constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in noop type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "noop") {
        BOOST_LOG_TRIVIAL(fatal) << "Noop constructor but yaml entry doesn't have type == noop";
        exit(EXIT_FAILURE);
    }

    filter = makeDoc(node["doc"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type noop";
}

// Execute the node
void noop::execute(mongocxx::client& conn, threadState& state) {
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    view.length();
}
}
