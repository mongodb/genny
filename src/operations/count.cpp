#include "count.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <bsoncxx/types/value.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include "node.hpp"

namespace mwg {

count::count(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = count
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Count constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in count type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "count") {
        BOOST_LOG_TRIVIAL(fatal) << "Count constructor but yaml entry doesn't have type == count";
        exit(EXIT_FAILURE);
    }
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type count";
    if (node["options"])
        parseCountOptions(options, node["options"]);
    if (node["assertEquals"])
        assertEquals = node["assertEquals"].as<int64_t>();
}

// Execute the node
void count::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn[state.DBName][state.CollectionName];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto returnCount = collection.count(view, options);
        if (assertEquals >= 0 && assertEquals != returnCount)
            BOOST_LOG_TRIVIAL(error) << "Count assertion error in node " << state.currentNode->name
                                     << ".Expected " << assertEquals << " but got " << returnCount;
        // save return count in state->return
        state.result = bsoncxx::builder::stream::array() << returnCount
                                                         << bsoncxx::builder::stream::finalize;
    } catch (mongocxx::operation_exception e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in count: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
}
}
