#include "update_many.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

update_many::update_many(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Update_Many constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in update_many type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "update_many") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Update_Many constructor but yaml entry doesn't have type == update_many";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseUpdateOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    update = makeDoc(node["update"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type update_many";
}

// Execute the node
void update_many::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    bsoncxx::builder::stream::document myupdate{};
    auto view = filter->view(mydoc, state);
    auto updateview = update->view(mydoc, state);
    auto result = collection.update_many(view, updateview, options);
    BOOST_LOG_TRIVIAL(debug) << "update_many.execute: update_many is " << bsoncxx::to_json(view);
}
}
