#include "overrideDocument.hpp"
#include <stdlib.h>

using namespace std;


namespace mwg {

overrideDocument::overrideDocument(YAML::Node& node) {
    if (!node) {
        cerr << "overrideDocument constructor and !node" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        cerr << "Not map in overrideDocument constructor" << endl;
        exit(EXIT_FAILURE);
    }
    if (!node["doc"]) {
        cerr << "No doc entry in yaml in overrideDocument constructor" << endl;
        exit(EXIT_FAILURE);
    }
    auto docnode = node["doc"];
    doc = move(bsonDocument(docnode));
    if (!node["overrides"]) {
        cerr << "No override entry in yaml in overrideDocument constructor" << endl;
        exit(EXIT_FAILURE);
    }
    for (auto entry : node["overrides"])
        override[entry.first.Scalar()] = entry.second.Scalar();
}

void overrideDocument::applyOverrideLevel(bsoncxx::builder::stream::document &output,
                                          bsoncxx::document::view doc,
                                          string prefix) {
    // Going to need variants of this for arrays

    // iterate through keys. if key matches exactly, replace in output.
    // if key doesn't match, copy element to output
    // if key prefix matches, descend a level.
    
    // process override for elements at this level
    unordered_map<string, string> thislevel;
    // process override for elements at lower level
    unordered_map<string, string> lowerlevel;

    for (auto elem : doc) {
        auto iter = thislevel.find(elem.key().to_string());
        auto iter2 = lowerlevel.find(elem.key().to_string());
        if (iter != thislevel.end())
            {
                // replace this entry
                output << elem.key().to_string() << iter->second;
            }
        else if (iter2 != lowerlevel.end())
            {
                //applyOverrideLevel(output, elem.value(), prefix + elem.key().to_string());
            }
        else {
            //doc << prefix + elem.key() << elem.value();
        }
    }
}

bsoncxx::document::view overrideDocument::get() {
    // Need to iterate through the doc, and for any field see if it
    // matches. Override the value if it does.
    // bson output
    bsoncxx::builder::stream::document output{};
    applyOverrideLevel(output, doc.get(), "");
    return output.view();
}
}
