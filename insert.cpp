#include "insert.hpp"
#include<stdlib.h>

namespace mwg {

    insert::insert(YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = insert
        if (!node) {
            cerr << "Insert constructor and !node" << endl;
            exit(EXIT_FAILURE);
            }
        if (!node.IsMap()) {
            cerr << "Not map in insert type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (node["type"].Scalar() != "insert") {
                cerr << "Insert constructor but yaml entry doesn't have type == insert" << endl;
                exit(EXIT_FAILURE);
            }
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
        cout << "In insert constructor. Name: " << name << ", nextName: " << nextName << endl;
        parseMap(insertdoc, node["document"]);
    }

    // Execute the node
    void insert::execute(mongocxx::client &conn) {
        auto collection = conn["testdb"]["testCollection"];
        auto result = collection.insert_one(insertdoc.view());
        // need a way to exhaust the cursor 
        cout << "insert.execute: insert is " << bsoncxx::to_json(insertdoc.view()) << endl;
        // probably should do some error checking here
    }
}
