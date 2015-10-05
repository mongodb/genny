#include "update_one.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

update_one::update_one(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Update_One constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in update_one type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "update_one") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Update_One constructor but yaml entry doesn't have type == update_one";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseUpdateOptions(options, node["options"]);
    filter = makeDoc(node["filter"]);
    update = makeDoc(node["update"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type update_one";
}

// Execute the node
void update_one::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    bsoncxx::builder::stream::document myupdate{};
    auto view = filter->view(mydoc, state);
    auto updateview = update->view(mydoc, state);
    auto result = collection.update_one(view, updateview, options);
    BOOST_LOG_TRIVIAL(debug) << "update_one.execute: update_one is " << bsoncxx::to_json(view);
}
}
