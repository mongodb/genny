#include "query.hpp"
#include<stdlib.h>

namespace mwg {

    query::query(YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = query
        if (!node) {
            cerr << "Query constructor and !node" << endl;
            exit(EXIT_FAILURE);
            }
        if (!node.IsMap()) {
            cerr << "Not map in query type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (node["type"].Scalar() != "query") {
                cerr << "Query constructor but yaml entry doesn't have type == query" << endl;
                exit(EXIT_FAILURE);
            }
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
        //cout << "In query constructor. Name: " << name << ", nextName: " << nextName << endl;
        parseMap(querydoc, node["query"]);
    }

    // Execute the node
    void query::execute(mongocxx::client &conn, mt19937_64 &) {
        auto collection = conn["testdb"]["testCollection"];
        auto cursor = collection.find(querydoc.view());
        // need a way to exhaust the cursor 
        cout << "query.execute: query is " << bsoncxx::to_json(querydoc.view()) << endl;
       for (auto&& doc : cursor) {
            std::cout << bsoncxx::to_json(doc) << std::endl;
        }
       cout << "After iterating results" << endl;
    }
}
