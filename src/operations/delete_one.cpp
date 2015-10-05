#include "delete_one.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

delete_one::delete_one(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = delete_one
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Delete_One constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in delete_one type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "delete_one") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Delete_One constructor but yaml entry doesn't have type == delete_one";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseDeleteOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type delete_one";
}

// Execute the node
void delete_one::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    auto result = collection.delete_many(view, options);
    BOOST_LOG_TRIVIAL(debug) << "delete_one.execute: delete_one is "
                             << bsoncxx::to_json(filter->view(mydoc, state));
}
}
