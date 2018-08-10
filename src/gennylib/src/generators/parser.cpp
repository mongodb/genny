#include "parser.hh"

#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <utility>

#include <bsoncxx/builder/concatenate.hpp>
#include <bsoncxx/json.hpp>

using bsoncxx::builder::concatenate;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace genny::generators::parser {

// Check for valid json number. This regex should match the diagram
// http://www.json.org/
bool isNumber(std::string value) {
    boost::regex re("-?(([1-9][0-9]*)|0)([.][0-9]*)?([eE][+-]?[0-9]+)?");
    if (boost::regex_match(value, re))
        return true;
    else
        return false;
}

bool isBool(std::string value) {
    if (value.compare("true") == 0)
        return true;
    if (value.compare("false") == 0)
        return true;
    return false;
}

// Surround by quotes if appropriate.
std::string quoteIfNeeded(std::string value) {
    // is it already quoted? Return as is
    if (value[0] == '"' && value[value.length() - 1] == '"')
        return value;
    // Is it a number? Return as is
    // should generalize the isNumber logic
    if (isNumber(value))
        return value;
    // Is it a boolean? Return as is
    if (isBool(value))
        return value;
    return "\"" + value + "\"";
}

void checkTemplates(std::string key,
                    YAML::Node& entry,
                    std::set<std::string>& templates,
                    std::string prefix,
                    std::vector<std::tuple<std::string, std::string, YAML::Node>>& overrides) {
    if (templates.count(key) > 0) {
        // We matched a template
        BOOST_LOG_TRIVIAL(trace) << "Matched a template. Make a note of it. Key is " << key;
        // After this logic works it should be pulled out into a function and used
        // here, in parseSequence, and in the scalar case below
        std::string path;
        if (prefix.length() > 0) {
            path = prefix.substr(0, prefix.length() - 1);
        } else {
            path = "";
            BOOST_LOG_TRIVIAL(warning) << "In checkTemplates and path is empty";
        }
        BOOST_LOG_TRIVIAL(trace) << "Pushing override for name: " << path << " and entry " << entry;
        overrides.push_back(std::make_tuple(path, key, entry));
    }
}

bsoncxx::document::value parseMap(
    YAML::Node node,
    std::set<std::string> templates,
    std::string prefix,
    std::vector<std::tuple<std::string, std::string, YAML::Node>>& overrides) {
    bsoncxx::builder::stream::document docbuilder{};
    BOOST_LOG_TRIVIAL(trace) << "In parseMap and prefix is " << prefix;
    for (auto entry : node) {
        auto key = entry.first.Scalar();
        BOOST_LOG_TRIVIAL(trace) << "About to call checkTemplates and key is " << key;
        checkTemplates(key, entry.second, templates, prefix, overrides);
        if (entry.second.IsMap()) {
            docbuilder << key << open_document
                       << concatenate(
                              parseMap(entry.second, templates, prefix + key + ".", overrides))
                       << close_document;
        } else if (entry.second.IsSequence()) {
            docbuilder << key << open_array
                       << concatenate(
                              parseSequence(entry.second, templates, prefix + key + ".", overrides))
                       << close_array;
        } else {  // scalar
            auto newPrefix = prefix + key + ".";
            auto newKey = entry.second.Scalar();
            BOOST_LOG_TRIVIAL(trace) << "About to call checkTemplates on scalar and key is " << key
                                     << ", newKey is " << newKey << " and prefix is " << newPrefix;
            checkTemplates(newKey, entry.second, templates, newPrefix, overrides);
            std::string doc = "{\"" + key + "\": " + quoteIfNeeded(entry.second.Scalar()) + "}";
            BOOST_LOG_TRIVIAL(trace)
                << "In parseMap and have scalar. Doc to pass to from_json: " << doc;
            docbuilder << concatenate(bsoncxx::from_json(doc));
        }
    }
    return docbuilder << finalize;
}

bsoncxx::document::value parseMap(YAML::Node node) {
    // empty templates, and will throw away overrides
    std::set<std::string> templates;
    std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;
    return (parseMap(node, templates, "", overrides));
}

bsoncxx::array::value parseSequence(
    YAML::Node node,
    std::set<std::string> templates,
    std::string prefix,
    std::vector<std::tuple<std::string, std::string, YAML::Node>>& overrides) {
    bsoncxx::builder::stream::array arraybuilder{};
    for (auto entry : node) {
        if (entry.IsMap()) {
            arraybuilder << open_document << concatenate(parseMap(entry)) << close_document;
        } else if (entry.IsSequence()) {
            arraybuilder << open_array << concatenate(parseSequence(entry)) << close_array;
        } else  // scalar
        {
            std::string doc = "{\"\": " + quoteIfNeeded(entry.Scalar()) + "}";
            BOOST_LOG_TRIVIAL(trace)
                << "In parseSequence and have scalar. Doc to pass to from_json: " << doc;
            arraybuilder << bsoncxx::from_json(doc).view()[""].get_value();
        }
    }
    return arraybuilder << finalize;
}

bsoncxx::array::value parseSequence(YAML::Node node) {
    // empty templates, and will throw away overrides
    std::set<std::string> templates;
    std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;
    return (parseSequence(node, templates, "", overrides));
}

bsoncxx::array::value yamlToValue(YAML::Node node) {
    bsoncxx::builder::stream::array myArray{};
    if (node.IsScalar()) {
        std::string doc = "{\"\": " + quoteIfNeeded(node.Scalar()) + "}";
        BOOST_LOG_TRIVIAL(trace) << "In yamlToValue and have scalar. Doc to pass to from_json: "
                                 << doc;
        BOOST_LOG_TRIVIAL(trace) << "In yamlToValue and have scalar. Original value is : "
                                 << node.Scalar();
        myArray << bsoncxx::from_json(doc).view()[""].get_value();
    } else if (node.IsSequence()) {
        myArray << open_array << concatenate(parseSequence(node)) << close_array;
    } else {  // MAP
        myArray << open_document << concatenate(parseMap(node)) << close_document;
    }
    return (myArray << bsoncxx::builder::stream::finalize);
}

}  // namespace genny::generators::parser
