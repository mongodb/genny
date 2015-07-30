#include "insert_many.hpp"
#include <stdlib.h>
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>

namespace mwg {

insert_many::insert_many(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = insert_many
    if (!node) {
        cerr << "Insert_Many constructor and !node" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        cerr << "Not map in insert_many type initializer" << endl;
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "insert_many") {
        cerr << "Insert_Many constructor but yaml entry doesn't have type == "
                "insert_many" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node["container"].IsSequence()) {
        cerr << "Insert_Many constructor but yaml entry for container isn't a "
                "sequence" << endl;
        exit(EXIT_FAILURE);
    }
    if (node["options"])
        parseInsertOptions(options, node["options"]);

    for (auto doc : node["container"]) {
        bsoncxx::builder::stream::document document;
        parseMap(document, doc);
        collection.push_back(move(document));
    }
    cout << "Added op of type insert_many" << endl;
}

// Execute the node
void insert_many::execute(mongocxx::client& conn, mt19937_64& rng) {
    auto coll = conn["testdb"]["testCollection"];
    auto result = coll.insert_many(collection, options);
    cout << "insert_many.execute" << endl;
    // probably should do some error checking here
}
}
