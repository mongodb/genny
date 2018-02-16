#include "load_file_node.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <fstream>
#include <iostream>
#include <mongocxx/exception/operation_exception.hpp>

#include "parse_util.hpp"

namespace mwg {
LoadFileNode::LoadFileNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "load_file") {
        BOOST_LOG_TRIVIAL(fatal)
            << "LoadFileNode constructor but yaml entry doesn't have type == load_file";
        exit(EXIT_FAILURE);
    }
    // proper path name
    auto fileName = ynode["file_name"].Scalar();
    // If path is set, use it
    if (ynode["path"]) {
        BOOST_LOG_TRIVIAL(trace) << "In LoadFileNode and path string is " << ynode["path"].Scalar();
        filePath = boost::filesystem::path(ynode["path"].Scalar());
        BOOST_LOG_TRIVIAL(trace) << "In LoadFileNode and path is " << filePath;
        filePath /= fileName;
        BOOST_LOG_TRIVIAL(trace) << "In LoadFileNode and path with file is " << filePath;
    } else {
        filePath = boost::filesystem::path(fileName);
    }
    if (ynode["options"])
        parseInsertOptions(options, ynode["options"]);
}

// Execute the node
void LoadFileNode::execute(shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(debug) << "LoadFileNode.execute";
    auto collection = myState->conn[myState->DBName][myState->CollectionName];

    ifstream loadFile(filePath.c_str());
    // check for no such file
    string line;
    while (getline(loadFile, line)) {
        try {
            BOOST_LOG_TRIVIAL(trace) << "In LoadFileNode and read in line " << line;
            collection.insert_one(bsoncxx::from_json(line), options);
        } catch (mongocxx::operation_exception e) {
            recordException();
            BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in insert_one: " << e.what();
            auto error = e.raw_server_error();
            if (error)
                BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        } catch (bsoncxx::exception e) {
            recordException();
            BOOST_LOG_TRIVIAL(error) << "Caught bsoncxx exception in from_json: " << e.what();
        }
    }
    // Catch mongoexception and file exception?
}
}
