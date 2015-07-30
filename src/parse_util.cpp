#include "parse_util.hpp"
#include <mongocxx/write_concern.hpp>
#include <chrono>

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using mongocxx::write_concern;

namespace mwg {

void parseMap(document& docbuilder, YAML::Node node) {
    for (auto entry : node) {
        cout << "In parseMap. entry.first: " << entry.first.Scalar() << " entry.second "
             << entry.second.Scalar() << endl;
        // can I just use basic document builder. Open, append, concatenate, etc?
        if (entry.second.IsMap()) {
            document mydoc{};
            parseMap(mydoc, entry.second);
            bsoncxx::builder::stream::concatenate doc;
            doc.view = mydoc.view();
            docbuilder << entry.first.Scalar() << open_document << doc << close_document;
        } else if (entry.second.IsSequence()) {
            bsoncxx::v0::builder::stream::array myArray{};
            parseSequence(myArray, entry.second);
            bsoncxx::builder::stream::concatenate doc;
            doc.view = myArray.view();
            docbuilder << entry.first.Scalar() << open_array << doc << close_array;
        } else {  // scalar
            docbuilder << entry.first.Scalar() << entry.second.Scalar();
        }
    }
}

void parseSequence(bsoncxx::v0::builder::stream::array& arraybuilder, YAML::Node node) {
    for (auto entry : node) {
        if (entry.IsMap()) {
            cout << "Entry isMap" << endl;
            document mydoc{};
            parseMap(mydoc, entry);
            bsoncxx::builder::stream::concatenate doc;
            doc.view = mydoc.view();
            arraybuilder << open_document << doc << close_document;
        } else if (entry.IsSequence()) {
            bsoncxx::v0::builder::stream::array myArray{};
            parseSequence(myArray, entry);
            bsoncxx::builder::stream::concatenate doc;
            doc.view = myArray.view();
            arraybuilder << open_array << doc << close_array;
        } else  // scalar
        {
            cout << "Trying to put entry into array builder " << entry.Scalar() << endl;
            arraybuilder << entry.Scalar();
        }
    }
}

void parseInsertOptions(mongocxx::options::insert& options, YAML::Node optionsNode) {
    if (optionsNode["write_concern"]) {
        write_concern wc {};
        // Need to set the options of the write concern
        if (optionsNode["fsync"])
            wc.fsync(optionsNode["fsync"].as<bool>());
        if (optionsNode["journal"])
            wc.journal(optionsNode["journal"].as<bool>());
        if (optionsNode["nodes"])
            wc.nodes(optionsNode["nodes"].as<int32_t>());
        // not sure how to handle this one. The parameter is different
        // than the option. Need to review the crud spec. Need more
        // error checking here also
        if (optionsNode["majority"])
            wc.majority(chrono::milliseconds(optionsNode["majority"]["timeout"].as<int64_t>()));
        if (optionsNode["tag"])
            wc.tag(optionsNode["tag"].Scalar());
        if (optionsNode["timeout"])
            wc.majority(chrono::milliseconds(optionsNode["timeout"].as<int64_t>()));
        options.write_concern(wc);
    }
}


}
