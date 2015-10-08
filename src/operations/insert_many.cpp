#include "insert_many.hpp"
#include <stdlib.h>
#include <bsoncxx/json.hpp>
#include "parse_util.hpp"
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>
#include <tuple>

namespace mwg {

insert_many::insert_many(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = insert_many
    if (!ynode) {
        BOOST_LOG_TRIVIAL(fatal) << "Insert_Many constructor and !ynode";
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in insert_many type initializer";
        exit(EXIT_FAILURE);
    }
    if (ynode["type"].Scalar() != "insert_many") {
        BOOST_LOG_TRIVIAL(fatal) << "Insert_Many constructor but yaml entry doesn't have type == "
                                    "insert_many";
        exit(EXIT_FAILURE);
    }
    if (ynode["options"])
        parseInsertOptions(options, ynode["options"]);

    if (ynode["container"]) {
        for (auto doc : ynode["container"]) {
            collection.push_back(makeDoc(doc));  // need to update execute to work with this
            use_collection = true;
        }
    } else if (ynode["doc"] && ynode["times"]) {
        BOOST_LOG_TRIVIAL(debug) << "In insert_many and have a doc and times";
        doc = makeDoc(ynode["doc"]);
        BOOST_LOG_TRIVIAL(debug) << "In insert_many and parsed doc";
        times = ynode["times"].as<uint64_t>();
        BOOST_LOG_TRIVIAL(debug) << "In insert_many and have times";
        use_collection = false;
    } else
        BOOST_LOG_TRIVIAL(fatal) << "In insert_many and don't have container or doc and times";
    BOOST_LOG_TRIVIAL(debug) << "Added op of type insert_many. ";
}

// Execute the node
void insert_many::execute(mongocxx::client& conn, threadState& state) {
    auto coll = conn["testdb"]["testCollection"];
    // need to transform this into a vector of bson documents for switch over to document class
    // make a vector of views and pass that in
    // The vector of documents is to save the state needed for the view if the collection of
    // documents is repeated calls to view of the same document
    uint64_t size;
    if (use_collection)
        size = collection.size();
    else
        size = times;
    vector<bsoncxx::builder::stream::document> docs(size);
    vector<bsoncxx::document::view> views;
    // copy over the documents into bsoncxx documents
    auto newDoc = docs.begin();
    if (use_collection) {
        for (auto doc = collection.begin(); doc != collection.end(); doc++) {
            views.push_back((*doc)->view(*newDoc, state));
            newDoc++;
        }
    } else {
        for (uint64_t i = 0; i < times; i++) {
            views.push_back(doc->view(*newDoc, state));
            newDoc++;
        }
    }
    try {
        auto result = coll.insert_many(views, options);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in insert_many: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value());
    }
    BOOST_LOG_TRIVIAL(debug) << "insert_many.execute";
    // probably should do some error checking here
}
}
