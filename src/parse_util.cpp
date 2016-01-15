#include "parse_util.hpp"
#include <mongocxx/write_concern.hpp>
#include <bsoncxx/builder/concatenate.hpp>

#include <chrono>
#include <boost/regex.hpp>  // STDLIB regex failed on Ubuntu 14.04 & CentOS 7
#include <boost/log/trivial.hpp>
#include <chrono>

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::concatenate;
using bsoncxx::builder::stream::finalize;
using mongocxx::write_concern;

namespace mwg {

// This could be made much cleaner. Ideally check for all the various valid regex and return a value
// that can be directly dropped into the builder.
bool isNumber(string value) {
    boost::regex re("-?[0-9]+");
    if (boost::regex_match(value, re))
        return true;
    else
        return false;
}

bsoncxx::document::value parseMap(YAML::Node node) {
    bsoncxx::builder::stream::document docbuilder{};
    for (auto entry : node) {
        if (entry.second.IsMap()) {
            docbuilder << entry.first.Scalar() << open_document
                       << concatenate(parseMap(entry.second)) << close_document;
        } else if (entry.second.IsSequence()) {
            docbuilder << entry.first.Scalar() << open_array
                       << concatenate(parseSequence(entry.second)) << close_array;
        } else {  // scalar
            if (isNumber(entry.second.Scalar())) {
                BOOST_LOG_TRIVIAL(debug) << "Value is a number according to our regex";
                docbuilder << entry.first.Scalar() << entry.second.as<int64_t>();
            } else
                docbuilder << entry.first.Scalar() << entry.second.Scalar();
        }
    }
    return docbuilder << finalize;
}

bsoncxx::array::value parseSequence(YAML::Node node) {
    bsoncxx::builder::stream::array arraybuilder{};
    for (auto entry : node) {
        if (entry.IsMap()) {
            arraybuilder << open_document << concatenate(parseMap(entry)) << close_document;
        } else if (entry.IsSequence()) {
            arraybuilder << open_array << concatenate(parseSequence(entry)) << close_array;
        } else  // scalar
        {
            if (isNumber(entry.Scalar())) {
                BOOST_LOG_TRIVIAL(debug) << "Value is a number according to our regex";
                arraybuilder << entry.as<int64_t>();
            } else
                arraybuilder << entry.Scalar();
        }
    }
    return arraybuilder << finalize;
}

bsoncxx::array::value yamlToValue(YAML::Node node) {
    if (!node.IsScalar()) {
        BOOST_LOG_TRIVIAL(fatal) << "yamlToValue and passed in non-scalar";
    }
    bsoncxx::builder::stream::array myArray{};
    if (isNumber(node.Scalar())) {
        myArray << node.as<int64_t>();
    } else {  // string
        myArray << node.Scalar();
    }
    return (std::move(myArray << bsoncxx::builder::stream::finalize));
}

write_concern parseWriteConcern(YAML::Node node) {
    write_concern wc{};
    // Need to set the options of the write concern
    if (node["fsync"])
        wc.fsync(node["fsync"].as<bool>());
    if (node["journal"])
        wc.journal(node["journal"].as<bool>());
    if (node["nodes"]) {
        wc.nodes(node["nodes"].as<int32_t>());
        BOOST_LOG_TRIVIAL(debug) << "Setting nodes to " << node["nodes"].as<int32_t>();
    }
    // not sure how to handle this one. The parameter is different
    // than the option. Need to review the crud spec. Need more
    // error checking here also
    if (node["majority"])
        wc.majority(chrono::milliseconds(node["majority"]["timeout"].as<int64_t>()));
    if (node["tag"])
        wc.tag(node["tag"].Scalar());
    if (node["timeout"])
        wc.majority(chrono::milliseconds(node["timeout"].as<int64_t>()));
    return wc;
}

void parseCreateCollectionOptions(mongocxx::options::create_collection& options, YAML::Node node) {
    if (node["capped"])
        options.capped(node["capped"].as<bool>());
    if (node["auto_index_id"])
        options.auto_index_id(node["auto_index_id"].as<bool>());
    if (node["size"])
        options.size(node["size"].as<int>());
    if (node["max"])
        options.max(node["max"].as<int>());
    // skipping storage engine options
    if (node["no_padding"])
        options.no_padding(node["no_padding"].as<bool>());
}

void parseIndexOptions(mongocxx::options::index& options, YAML::Node node) {
    if (node["background"])
        options.background(node["background"].as<bool>());
    if (node["unique"])
        options.unique(node["unique"].as<bool>());
    if (node["name"])
        options.name(node["name"].Scalar());
    if (node["sparse"])
        options.sparse(node["sparse"].as<bool>());
    // Skipping storage options
    if (node["expire_after_seconds"])
        options.expire_after_seconds(node["expire_after_seconds"].as<std::int32_t>());
    if (node["version"])
        options.version(node["version"].as<std::int32_t>());
    if (node["weights"]) {
        options.weights(parseMap(node["weights"]));
    }
    if (node["default_language"])
        options.default_language(node["default_language"].Scalar());
    if (node["language_override"])
        options.language_override(node["language_override"].Scalar());
    if (node["partial_filter_expression"]) {
        options.partial_filter_expression(parseMap(node["partial_filter_expression"]));
    }
    if (node["twod_sphere_version"])
        options.twod_sphere_version(node["twod_sphere_version"].as<std::uint8_t>());
    if (node["twod_bits_precision"])
        options.twod_bits_precision(node["twod_bits_precision"].as<std::uint8_t>());
    if (node["twod_location_min"])
        options.twod_location_min(node["twod_location_min"].as<double>());
    if (node["twod_location_max"])
        options.twod_location_max(node["twod_location_max"].as<double>());
    if (node["haystack_bucket_size"])
        options.haystack_bucket_size(node["haystack_bucket_size"].as<double>());
}


void parseInsertOptions(mongocxx::options::insert& options, YAML::Node node) {
    if (node["write_concern"])
        options.write_concern(parseWriteConcern(node["write_concern"]));
}


void parseCountOptions(mongocxx::options::count& options, YAML::Node node) {
    if (node["hint"]) {
        options.hint(mongocxx::hint(parseMap(node["hint"])));
    }
    if (node["limit"])
        options.limit(node["limit"].as<int32_t>());
    if (node["max_time"])
        options.max_time(std::chrono::milliseconds(node["max_time"].as<int64_t>()));
    if (node["read_preference"]) {
        options.read_preference(parseReadPreference(node["read_preference"]));
    }
    if (node["skip"])
        options.skip(node["skip"].as<int32_t>());
}
void parseAggregateOptions(mongocxx::options::aggregate& options, YAML::Node node) {
    if (node["allow_disk_use"])
        options.allow_disk_use(node["allow_disk_use"].as<bool>());
    if (node["batch_size"])
        options.batch_size(node["batch_size"].as<int32_t>());
    if (node["max_time"])
        options.max_time(std::chrono::milliseconds(node["max_time"].as<int64_t>()));
    if (node["use_cursor"])
        options.use_cursor(node["use_cursor"].as<bool>());
    if (node["read_preference"]) {
        options.read_preference(parseReadPreference(node["read_preference"]));
    }
}
void parseBulkWriteOptions(mongocxx::options::bulk_write& options, YAML::Node node) {
    if (node["ordered"])
        options.ordered(node["ordered"].as<bool>());
    if (node["write_concern"])
        options.write_concern(parseWriteConcern(node["write_concern"]));
}
void parseDeleteOptions(mongocxx::options::delete_options& options, YAML::Node node) {
    if (node["write_concern"])
        options.write_concern(parseWriteConcern(node["write_concern"]));
}
void parseDistinctOptions(mongocxx::options::distinct& options, YAML::Node node) {
    if (node["max_time"])
        options.max_time(std::chrono::milliseconds(node["max_time"].as<int64_t>()));
    if (node["read_preference"]) {
        options.read_preference(parseReadPreference(node["read_preference"]));
    }
}
void parseFindOptions(mongocxx::options::find& options, YAML::Node node) {
    BOOST_LOG_TRIVIAL(debug) << "In parseFindOptions";
    if (node["allow_partial_results"])
        options.allow_partial_results(node["allow_partial_results"].as<bool>());
    if (node["batch_size"])
        options.batch_size(node["batch_size"].as<int32_t>());
    if (node["comment"])
        options.comment(node["comment"].Scalar());
    // skipping cursor type for now. It's just an enum. Just need to match them.
    if (node["limit"])
        options.limit(node["limit"].as<int32_t>());
    if (node["max_time"])
        options.max_time(std::chrono::milliseconds(node["max_time"].as<int64_t>()));
    if (node["modifiers"]) {
        options.modifiers(parseMap(node["modifiers"]));
    }
    if (node["no_cursor_timeout"])
        options.no_cursor_timeout(node["no_cursor_timeout"].as<bool>());
    if (node["projection"]) {
        options.projection(parseMap(node["projection"]));
    }
    if (node["read_preference"]) {
        options.read_preference(parseReadPreference(node["read_preference"]));
    }
    if (node["skip"])
        options.skip(node["skip"].as<int32_t>());
    if (node["sort"]) {
        options.sort(parseMap(node["sort"]));
    }
}
void parseFindOneAndDeleteOptions(mongocxx::options::find_one_and_delete& options,
                                  YAML::Node node) {
    // if (node["max_time_ms"])
    //     options.max_time_ms(node["max_time_ms"].as<int64_t>());
    if (node["projection"]) {
        options.projection(parseMap(node["projection"]));
    }
    if (node["sort"]) {
        options.sort(parseMap(node["sort"]));
    }
}
void parseFindOneAndReplaceOptions(mongocxx::options::find_one_and_replace& options,
                                   YAML::Node node) {
    // if (node["max_time_ms"])
    //     options.max_time_ms(node["max_time_ms"].as<int64_t>());
    if (node["projection"]) {
        options.projection(parseMap(node["projection"]));
    }
    if (node["sort"]) {
        options.sort(parseMap(node["sort"]));
    }
    // if (node["return_document"})
    //     {}// Need to fill this one in
    if (node["upsert"])
        options.upsert(node["upsert"].as<bool>());
}
void parseFindOneAndUpdateOptions(mongocxx::options::find_one_and_update& options,
                                  YAML::Node node) {
    if (node["projection"]) {
        options.projection(parseMap(node["projection"]));
    }
    if (node["sort"]) {
        options.sort(parseMap(node["sort"]));
    }
    // if (node["return_document"})
    //     {}// Need to fill this one in
    if (node["upsert"])
        options.upsert(node["upsert"].as<bool>());
}
void parseUpdateOptions(mongocxx::options::update& options, YAML::Node node) {
    if (node["upsert"])
        options.upsert(node["upsert"].as<bool>());
    if (node["write_concern"])
        options.write_concern(parseWriteConcern(node["write_concern"]));
}

mongocxx::read_preference parseReadPreference(YAML::Node node) {
    mongocxx::read_preference pref;
    if (node["mode"]) {
        if (node["mode"].Scalar() == "primary")
            pref.mode(mongocxx::read_preference::read_mode::k_primary);
        else if (node["mode"].Scalar() == "primary_preferred")
            pref.mode(mongocxx::read_preference::read_mode::k_primary_preferred);
        else if (node["mode"].Scalar() == "secondary")
            pref.mode(mongocxx::read_preference::read_mode::k_secondary);
        else if (node["mode"].Scalar() == "secondary_preferred")
            pref.mode(mongocxx::read_preference::read_mode::k_secondary_preferred);
        else if (node["mode"].Scalar() == "nearest")
            pref.mode(mongocxx::read_preference::read_mode::k_nearest);
    }
    if (node["tags"]) {
        pref.tags(parseMap(node["tags"]));
    }

    return pref;
}
}
