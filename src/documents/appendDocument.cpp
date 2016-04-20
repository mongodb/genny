#include "appendDocument.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>
#include <ctime>
#include <stdlib.h>

#include "value_generators.hpp"
#include "workload.hpp"

using namespace std;

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;

namespace mwg {

AppendDocument::AppendDocument(YAML::Node& node) : document() {
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "AppendDocument constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in AppendDocument constructor";
        exit(EXIT_FAILURE);
    }
    if (auto docnode = node["doc"]) {
        doc = bsonDocument(docnode);
    } else {
        doc = bsonDocument();
    }

    if (node["appends"]) {
        // just save the whole document that is second for now
        for (auto entry : node["appends"])
            appends.push_back(
                make_pair(entry.first.Scalar(), makeUniqueValueGenerator(entry.second)));
    }
}
bsoncxx::document::view AppendDocument::view(bsoncxx::builder::stream::document& output,
                                             threadState& state) {
    bsoncxx::builder::stream::document tempdoc{};
    output << bsoncxx::builder::concatenate(doc.view(output, state));
    for (auto&& entry : appends) {
        BOOST_LOG_TRIVIAL(trace) << "Adding field " << entry.first;
        output << entry.first << entry.second->generate(state).view()[0].get_value();
    }

    return output.view();
}
}
