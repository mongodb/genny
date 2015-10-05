#include "run_command.hpp"
#include "parse_util.hpp"
#include "../documents/bsonDocument.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>

namespace mwg {

run_command::run_command(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = run_command
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Run_Command constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in run_command type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "run_command") {
        BOOST_LOG_TRIVIAL(fatal) << "Run_Command constructor but yaml entry doesn't have "
                                    "type == run_command";
        exit(EXIT_FAILURE);
    }
    command = makeDoc(node["command"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type run_command";
}

// Execute the node
void run_command::execute(mongocxx::client& conn, threadState& state) {
    auto db = conn["testdb"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = command->view(mydoc, state);
    db.command(view);
    // need a way to exhaust the cursor
    BOOST_LOG_TRIVIAL(debug) << "run_command.execute: run_command with command "
                             << bsoncxx::to_json(view);
}
}
