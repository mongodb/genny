#include "insert_one.hpp"
#include <stdlib.h>
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>

namespace mwg {

insert_one::insert_one(YAML::Node& node) {
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
        cerr << "Insert_One constructor but yaml entry doesn't have type == "
                "insert_one" << endl;
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseInsertOptions(options, node["options"]);

    //    parseMap(document, node["document"]);
    document = makeDoc(node["document"]);
    cout << "Added op of type insert_one" << endl;
}

// Execute the node
void insert_one::execute(mongocxx::client& conn, mt19937_64& rng) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto result = collection.insert_one(document->view(mydoc), options);
    // need a way to exhaust the cursor
    cout << "insert_one.execute: insert_one is " << bsoncxx::to_json(document->view(mydoc)) << endl;
    // probably should do some error checking here
}
}
