#include "overrideDocument.hpp"
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types/value.hpp>
#include <stdlib.h>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>
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

void overrideDocument::applyOverrideLevel(bsoncxx::builder::stream::document& output,
                                          bsoncxx::document::view doc,
                                          string prefix) {
    //     applyOverrideLevel(output, doc.begin(), doc.end(), prefix);
    // }

    //     void applyOverrideLevel(bsoncxx::builder::stream::document& output,
    //     bsoncxx::document::view::iterator begin, bsoncxx::document::view::iterator end, string
    //     prefix) {

    // Going to need variants of this for arrays

    // iterate through keys. if key matches exactly, replace in output.
    // if key doesn't match, copy element to output
    // if key prefix matches, descend a level.

    // process override for elements at this level
    unordered_map<string, string> thislevel;
    // process override for elements at lower level
    unordered_map<string, string> lowerlevel;

    for (auto elem : override) {
        string key = elem.first;
        if (key.compare(0, prefix.length(), prefix)) {
            // prefix match. Need what comes after
            // grab everything after prefix
            auto suffix = key.substr(prefix.length(),key.length() - prefix.length());
        }
    }

    for (auto elem : doc) {
        //    for (auto elem = begin; elem != end; elem++) {
        cout << "Looking at key " << elem.key().to_string() << endl;
        auto iter = thislevel.find(elem.key().to_string());
        auto iter2 = lowerlevel.find(elem.key().to_string());
        if (iter != thislevel.end()) {
            // replace this entry
            cout << "Matched on this level. Replacing " << endl;
            output << elem.key().to_string() << iter->second;
        } else if (iter2 != lowerlevel.end()) {
            // need to check if child is document, array, or other.
            cout << "Partial match. Need to descend" << endl;
            switch (elem.type()) {
                case bsoncxx::type::k_document:
                    applyOverrideLevel(
                        output, elem.get_document().value, prefix + elem.key().to_string() + '.');
                    break;
                case bsoncxx::type::k_array:
                    cerr << "Trying to descend a level of bson in overrides. Array not supported "
                            "yet." << endl;
                default:
                    cerr << "Trying to descend a level of bson in overrides but not a map or array"
                         << endl;
                    exit(EXIT_FAILURE);
            }
        } else {
            cout << "No match, just pass through" << endl;
            bsoncxx::types::value ele_val{elem.get_value()};
            output << elem.key().to_string() << ele_val;
            cout << "No match, just passed through" << endl;
        }
        cout << "After if, elseif, else" << endl;
    }
}

bsoncxx::document::view overrideDocument::view(bsoncxx::builder::stream::document& output) {
    // Need to iterate through the doc, and for any field see if it
    // matches. Override the value if it does.
    // bson output

    // scope problem -- output is going out of scope here
    // to be thread safe this has to be on the stack or in the per thread data.

    // Not sure I need the tempdoc in addition to output
    bsoncxx::builder::stream::document tempdoc{};
    applyOverrideLevel(output, doc.view(tempdoc), "");
    cout << "About to give view" << endl;
    return output.view();
}
}
