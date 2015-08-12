#include "insert_many.hpp"
#include <stdlib.h>
#include <bsoncxx/json.hpp>
#include "parse_util.hpp"

namespace mwg {

insert_many::insert_many(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = insert_many
    if (!ynode) {
        cerr << "Insert_Many constructor and !ynode" << endl;
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        cerr << "Not map in insert_many type initializer" << endl;
        exit(EXIT_FAILURE);
    }
    if (ynode["type"].Scalar() != "insert_many") {
        cerr << "Insert_Many constructor but yaml entry doesn't have type == "
                "insert_many" << endl;
        exit(EXIT_FAILURE);
    }
    if (!ynode["container"].IsSequence()) {
        cerr << "Insert_Many constructor but yaml entry for container isn't a "
                "sequence" << endl;
        exit(EXIT_FAILURE);
    }
    if (ynode["options"])
        parseInsertOptions(options, ynode["options"]);

    for (auto doc : ynode["container"]) {
        bsoncxx::builder::stream::document document;
        parseMap(document, doc);
        collection.push_back(move(document));
        // collection.push_back(makeDoc(doc));  need to update execute to work with this
    }
    // cout << "Added op of type insert_many. WC.nodes is " << options.write_concern()->nodes()
    //<< endl;
}

// Execute the node
void insert_many::execute(mongocxx::client& conn, threadState& state) {
    auto coll = conn["testdb"]["testCollection"];
    // need to transform this into a vector of bson documents for switch over to document class
    auto result = coll.insert_many(collection, options);
    // cout << "insert_many.execute" << endl;
    // probably should do some error checking here
}
}
