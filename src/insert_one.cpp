#include "insert_one.hpp"
#include<stdlib.h>
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>


namespace mwg {

    insert_one::insert_one(YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = insert_one
        if (!node) {
            cerr << "Insert_One constructor and !node" << endl;
            exit(EXIT_FAILURE);
            }
        if (!node.IsMap()) {
            cerr << "Not map in insert_one type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (node["type"].Scalar() != "insert_one") {
                cerr << "Insert_One constructor but yaml entry doesn't have type == insert_one" << endl;
                exit(EXIT_FAILURE);
            }
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
        //        cout << "In insert_one constructor. Name: " << name << ", nextName: " << nextName << endl;
        parseMap(document, node["document"]);
    }

    // Execute the node
    void insert_one::execute(mongocxx::client &conn, mt19937_64 &rng) {
        auto collection = conn["testdb"]["testCollection"];
        auto result = collection.insert_one(document.view());
        // need a way to exhaust the cursor 
        cout << "insert_one.execute: insert_one is " << bsoncxx::to_json(document.view()) << endl;
        // probably should do some error checking here
    }
}
