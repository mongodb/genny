#include "insert_one.hpp"
#include <stdlib.h>
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

insert_one::insert_one(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = insert_one
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Insert_One constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in insert_one type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "insert_one") {
        BOOST_LOG_TRIVIAL(fatal) << "Insert_One constructor but yaml entry doesn't have type == "
                                    "insert_one";
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseInsertOptions(options, node["options"]);

    //    parseMap(document, node["document"]);
    doc = makeDoc(node["document"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type insert_one";
}

// Execute the node
void insert_one::execute(mongocxx::client& conn, threadState& state) {
    BOOST_LOG_TRIVIAL(trace) << "insert_one.execute before call";
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    BOOST_LOG_TRIVIAL(trace) << "insert_one.execute before call";
    // need to save the view to ensure we print out the same thing we insert
    auto view = doc->view(mydoc, state);
    try {
        auto result = collection.insert_one(view, options);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in insert_one: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "insert_one.execute: insert_one is " << bsoncxx::to_json(view);
    // probably should do some error checking here
}
}
