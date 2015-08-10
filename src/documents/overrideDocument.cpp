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
#include <boost/log/trivial.hpp>

using namespace std;

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

namespace mwg {

overrideDocument::overrideDocument(YAML::Node& node) {
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "overrideDocument constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in overrideDocument constructor";
        exit(EXIT_FAILURE);
    }
    if (!node["doc"]) {
        BOOST_LOG_TRIVIAL(fatal) << "No doc entry in yaml in overrideDocument constructor";
        exit(EXIT_FAILURE);
    }
    auto docnode = node["doc"];
    doc = move(bsonDocument(docnode));
    if (!node["overrides"]) {
        BOOST_LOG_TRIVIAL(fatal) << "No override entry in yaml in overrideDocument constructor";
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
    // Going to need variants of this for arrays

    // iterate through keys. if key matches exactly, replace in output.
    // if key doesn't match, copy element to output
    // if key prefix matches, descend a level.

    // process override for elements at this level
    unordered_map<string, YAML::Node> thislevel;
    // process override for elements at lower level
    set<string> lowerlevel;
    //    cout << "prefix is " << prefix ;
    for (auto elem : override) {
        string key = elem.first;
        BOOST_LOG_TRIVIAL(trace) << "Going through overrides key: " << key << " value is "
                                 << elem.second << " prefix.length() = " << prefix.length();
        if (prefix == "" || key.compare(0, prefix.length(), prefix) == 0) {
            // prefix match. Need what comes after
            // grab everything after prefix
            BOOST_LOG_TRIVIAL(trace) << "Key matched with prefix";
            auto suffix = key.substr(prefix.length(), key.length() - prefix.length());
            // check for a period. If no period, put in thislevel
            auto find = suffix.find('.');
            // no match
            if (find == std::string::npos) {
                thislevel[suffix] = elem.second;
                BOOST_LOG_TRIVIAL(trace) << "Putting thislevel[" << suffix << "]=" << elem.second;
            } else {
                // if period, grab from suffix to period and put in lowerlevel
                // We won't actually use the second element here
                BOOST_LOG_TRIVIAL(trace) << "Putting lowerlevel[" << suffix << "]=" << elem.second;
                lowerlevel.insert(suffix.substr(0, find));
            }
        } else {
            BOOST_LOG_TRIVIAL(trace) << "No prefix match";
        }
    }

    for (auto elem : doc) {
        BOOST_LOG_TRIVIAL(trace) << "Looking at key " << elem.key().to_string();
        auto iter = thislevel.find(elem.key().to_string());
        auto iter2 = lowerlevel.find(elem.key().to_string());
        if (iter != thislevel.end()) {
            // replace this entry
            BOOST_LOG_TRIVIAL(trace) << "Matched on this level. Replacing ";
            // this should all be replaced with a value class and polymorphism
            if (iter->second.IsScalar()) {
                output << elem.key().to_string() << iter->second.Scalar();
            }
            if (iter->second.IsMap()) {
                BOOST_LOG_TRIVIAL(trace) << "Second is map";
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
                    BOOST_LOG_TRIVIAL(trace) << "Override document and want to set field  "
                                             << iter->first << " with incremented value of "
                                             << iter->second["variable"].Scalar();
                    // if in tvariables use that
                    if (state.tvariables.count(iter->second["variable"].Scalar()) > 0) {
                        BOOST_LOG_TRIVIAL(trace) << "In tvariables";
                        output << elem.key().to_string()
                               << state.tvariables[iter->second["variable"].Scalar()]++;
                    } else {  // in wvariables
                        BOOST_LOG_TRIVIAL(trace) << "In wvariables";
                        output << elem.key().to_string()
                               << state.wvariables[iter->second["variable"].Scalar()]++;
                    }
                }
            }

        } else if (iter2 != lowerlevel.end()) {
            // need to check if child is document, array, or other.
            BOOST_LOG_TRIVIAL(trace) << "Partial match. Need to descend";
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
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Trying to descend a level of bson in overrides. Array not "
                           "supported "
                           "yet.";
                default:
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Trying to descend a level of bson in overrides but not a map or "
                           "array";
                    exit(EXIT_FAILURE);
            }
        } else {
            BOOST_LOG_TRIVIAL(trace) << "No match, just pass through";
            bsoncxx::types::value ele_val{elem.get_value()};
            output << elem.key().to_string() << ele_val;
        }
    }
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
    return output.view();
}
}
