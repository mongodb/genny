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

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

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
    // just save the whole document that is second for now
    for (auto entry : node["overrides"])
        override[entry.first.Scalar()] = entry.second;
}

void overrideDocument::applyOverrideLevel(bsoncxx::builder::stream::document& output,
                                          bsoncxx::document::view doc,
                                          string prefix,
                                          threadState& state) {
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
    unordered_map<string, YAML::Node> thislevel;
    // process override for elements at lower level
    set<string> lowerlevel;
    //    cout << "prefix is " << prefix << endl;
    for (auto elem : override) {
        string key = elem.first;
        // cout << "Going through overrides key: " << key << " value is " << elem.second
        //      << " prefix.length() = " << prefix.length() << endl;
        if (prefix == "" || key.compare(0, prefix.length(), prefix) == 0) {
            // prefix match. Need what comes after
            // grab everything after prefix
            // cout << "Key matched with prefix" << endl;
            auto suffix = key.substr(prefix.length(), key.length() - prefix.length());
            // check for a period. If no period, put in thislevel
            auto find = suffix.find('.');
            // no match
            if (find == std::string::npos) {
                thislevel[suffix] = elem.second;
                // cout << "Putting thislevel[" << suffix << "]=" << elem.second << endl;
            } else {
                // if period, grab from suffix to period and put in lowerlevel
                // We won't actually use the second element here
                // cout << "Putting lowerlevel[" << suffix << "]=" << elem.second << endl;
                lowerlevel.insert(suffix.substr(0, find));
            }
        } else {
            // cout << "No prefix match" << endl;
        }
    }

    for (auto elem : doc) {
        // cout << "Looking at key " << elem.key().to_string() << endl;
        auto iter = thislevel.find(elem.key().to_string());
        auto iter2 = lowerlevel.find(elem.key().to_string());
        if (iter != thislevel.end()) {
            // replace this entry
            // cout << "Matched on this level. Replacing " << endl;
            // this should all be replaced with a value class and polymorphism
            if (iter->second.IsScalar()) {
                output << elem.key().to_string() << iter->second.Scalar();
            }
            if (iter->second.IsMap()) {
                // cout << "Second is map" << endl;
                if (iter->second["type"].Scalar() == "randomint") {
                    int min = 0;
                    int max = 100;
                    if (iter->second["min"]) {
                        min = iter->second["min"].as<int>();
                    }
                    if (iter->second["max"]) {
                        max = iter->second["max"].as<int>();
                    }
                    uniform_int_distribution<int> distribution(min, max);
                    output << elem.key().to_string() << distribution(state.rng);
                } else if (iter->second["type"].Scalar() == "increment") {
                    // cout << "Override document and want to set field  " << iter->first
                    //      << " with incremented value of " << iter->second["variable"].Scalar()
                    //      << endl;
                    // if in tvariables use that
                    if (state.tvariables.count(iter->second["variable"].Scalar()) > 0) {
                        // cout << "In tvariables" << endl;
                        output << elem.key().to_string()
                               << state.tvariables[iter->second["variable"].Scalar()]++;
                    } else {  // in wvariables
                        // cout << "In wvariables" << endl;
                        output << elem.key().to_string()
                               << state.wvariables[iter->second["variable"].Scalar()]++;
                        // cout << "After wvariables" << endl;
                    }
                }
            }

        } else if (iter2 != lowerlevel.end()) {
            // need to check if child is document, array, or other.
            // cout << "Partial match. Need to descend" << endl;
            switch (elem.type()) {
                case bsoncxx::type::k_document: {
                    bsoncxx::builder::stream::document mydoc{};
                    applyOverrideLevel(mydoc,
                                       elem.get_document().value,
                                       prefix + elem.key().to_string() + '.',
                                       state);
                    bsoncxx::builder::stream::concatenate cdoc;
                    cdoc.view = mydoc.view();
                    output << elem.key().to_string() << open_document << cdoc << close_document;
                } break;
                case bsoncxx::type::k_array:
                    cerr << "Trying to descend a level of bson in overrides. Array not "
                            "supported "
                            "yet." << endl;
                default:
                    cerr << "Trying to descend a level of bson in overrides but not a map or "
                            "array" << endl;
                    exit(EXIT_FAILURE);
            }
        } else {
            // cout << "No match, just pass through" << endl;
            bsoncxx::types::value ele_val{elem.get_value()};
            output << elem.key().to_string() << ele_val;
            //  cout << "No match, just passed through" << endl;
        }
        // cout << "After if, elseif, else" << endl;
    }
    // cout << "After for loop" << endl;
}

bsoncxx::document::view overrideDocument::view(bsoncxx::builder::stream::document& output,
                                               threadState& state) {
    // Need to iterate through the doc, and for any field see if it
    // matches. Override the value if it does.
    // bson output

    // scope problem -- output is going out of scope here
    // to be thread safe this has to be on the stack or in the per thread data.

    // Not sure I need the tempdoc in addition to output
    bsoncxx::builder::stream::document tempdoc{};
    applyOverrideLevel(output, doc.view(tempdoc, state), "", state);
    // cout << "About to give view" << endl;
    return output.view();
}
}
