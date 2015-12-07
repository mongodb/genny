
#include "command.hpp"
#include "parse_util.hpp"
#include "../documents/bsonDocument.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

command::command(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = command
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Command constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in command type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "command") {
        BOOST_LOG_TRIVIAL(fatal) << "Command constructor but yaml entry doesn't have "
                                    "type == command";
        exit(EXIT_FAILURE);
    }
    myCommand = makeDoc(node["command"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type command";
}

// Execute the node
void command::execute(mongocxx::client& conn, threadState& state) {
    auto db = conn["testdb"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = myCommand->view(mydoc, state);
    try {
        db.run_command(view);
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in command: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
    }
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "command.execute: command with command " << bsoncxx::to_json(view);
}
}
