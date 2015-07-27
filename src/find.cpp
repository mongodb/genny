#include "find.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include<stdlib.h>

namespace mwg {

    find::find(YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = find
        if (!node) {
            cerr << "Find constructor and !node" << endl;
            exit(EXIT_FAILURE);
            }
        if (!node.IsMap()) {
            cerr << "Not map in find type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (node["type"].Scalar() != "find") {
                cerr << "Find constructor but yaml entry doesn't have type == find" << endl;
                exit(EXIT_FAILURE);
            }
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
        //cout << "In find constructor. Name: " << name << ", nextName: " << nextName << endl;
        parseMap(filter, node["find"]);
    }

    // Execute the node
    void find::execute(mongocxx::client &conn, mt19937_64 &) {
        auto collection = conn["testdb"]["testCollection"];
        auto cursor = collection.find(filter.view());
        // need a way to exhaust the cursor 
        cout << "find.execute: find is " << bsoncxx::to_json(filter.view()) << endl;
       for (auto&& doc : cursor) {
            std::cout << bsoncxx::to_json(doc) << std::endl;
        }
       cout << "After iterating results" << endl;
    }
}
