#include "value_generator.hpp"

#include <bsoncxx/types.hpp>
#include <boost/log/trivial.hpp>

#include "value_generators.hpp"

namespace mwg {

// This returns a set of the value generator types with $ prefixes
const std::set<std::string> getGeneratorTypes() {
    return (std::set<std::string>{"$add",
                                  "$choose",
                                  "$concatenate",
                                  "$date",
                                  "$increment",
                                  "$multiply",
                                  "$randomint",
                                  "$fastrandomstring",
                                  "$randomstring",
                                  "$useresult",
                                  "$useval",
                                  "$usevar"});
}


ValueGenerator* makeValueGenerator(YAML::Node yamlNode, std::string type) {
    if (type == "add") {
        return new AddGenerator(yamlNode);
    } else if (type == "choose") {
        return new ChooseGenerator(yamlNode);
    } else if (type == "concatenate") {
        return new ConcatenateGenerator(yamlNode);
    } else if (type == "date") {
        return new DateGenerator(yamlNode);
    } else if (type == "increment") {
        return new IncrementGenerator(yamlNode);
    } else if (type == "multiply") {
        return new MultiplyGenerator(yamlNode);
    } else if (type == "randomint") {
        return new RandomIntGenerator(yamlNode);
    } else if (type == "randomstring") {
        return new RandomStringGenerator(yamlNode);
    } else if (type == "fastrandomstring") {
        return new FastRandomStringGenerator(yamlNode);
    } else if (type == "useresult") {
        return new UseResultGenerator(yamlNode);
    } else if (type == "useval") {
        return new UseValueGenerator(yamlNode);
    } else if (type == "usevar") {
        return new UseVarGenerator(yamlNode);
    }
    BOOST_LOG_TRIVIAL(fatal) << "In makeValueGenerator and don't know how to handle type " << type;
    exit(EXIT_FAILURE);
}

ValueGenerator* makeValueGenerator(YAML::Node yamlNode) {
    if (yamlNode.IsScalar()) {
        return new UseValueGenerator(yamlNode);
    }
    // Should we put a list directly into UseValueGenerator also?
    if (!yamlNode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal)
            << "ValueGenerator Node in makeValueGenerator is not a yaml map or a sequence";
        exit(EXIT_FAILURE);
    }
    if (auto type = yamlNode["type"])
        return (makeValueGenerator(yamlNode, type.Scalar()));
    // If it doesn't have a type field, search for templating keys
    for (auto&& entry : yamlNode) {
        auto key = entry.first.Scalar();
        if (getGeneratorTypes().count(key)) {
            auto type = key.substr(1, key.length());
            return (makeValueGenerator(entry.second, type));
        }
    }
    return (makeValueGenerator(yamlNode, "useval"));
}

int64_t ValueGenerator::generateInt(threadState& state) {
    return valAsInt(generate(state));
};
double ValueGenerator::generateDouble(threadState& state) {
    return valAsDouble(generate(state));
};
std::string ValueGenerator::generateString(threadState& state) {
    return valAsString(generate(state));
}


std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node yamlNode) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode));
}
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node yamlNode) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode));
}

std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node yamlNode, std::string type) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode, type));
}
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node yamlNode, std::string type) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode, type));
}

// Check type cases and get a string out of it. Assumes it is getting a bson array of length 1.
std::string valAsString(view_or_value val) {
    auto elem = val.view()[0];
    switch (elem.type()) {
        case bsoncxx::type::k_int64:
            return (std::to_string(elem.get_int64().value));
            break;
        case bsoncxx::type::k_int32:
            return (std::to_string(elem.get_int32().value));
            break;
        case bsoncxx::type::k_double:
            return (std::to_string(elem.get_double().value));
            break;
        case bsoncxx::type::k_utf8:
            return (elem.get_utf8().value.to_string());
            break;
        case bsoncxx::type::k_document:
        case bsoncxx::type::k_array:
        case bsoncxx::type::k_binary:
        case bsoncxx::type::k_undefined:
        case bsoncxx::type::k_oid:
        case bsoncxx::type::k_bool:
        case bsoncxx::type::k_date:
        case bsoncxx::type::k_null:
        case bsoncxx::type::k_regex:
        case bsoncxx::type::k_dbpointer:
        case bsoncxx::type::k_code:
        case bsoncxx::type::k_symbol:
        case bsoncxx::type::k_timestamp:

            BOOST_LOG_TRIVIAL(fatal) << "valAsString with type unsuported type in list";
            exit(EXIT_FAILURE);
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "valAsString with type unsuported type not in list";
            exit(EXIT_FAILURE);
    }
    return ("");
}
int64_t valAsInt(view_or_value val) {
    auto elem = val.view()[0];
    switch (elem.type()) {
        case bsoncxx::type::k_int64:
            return (elem.get_int64().value);
            break;
        case bsoncxx::type::k_int32:
            return (static_cast<int64_t>(elem.get_int32().value));
            break;
        case bsoncxx::type::k_double:
            return (static_cast<int64_t>(elem.get_double().value));
            break;
        case bsoncxx::type::k_utf8:
        case bsoncxx::type::k_document:
        case bsoncxx::type::k_array:
        case bsoncxx::type::k_binary:
        case bsoncxx::type::k_undefined:
        case bsoncxx::type::k_oid:
        case bsoncxx::type::k_bool:
        case bsoncxx::type::k_date:
        case bsoncxx::type::k_null:
        case bsoncxx::type::k_regex:
        case bsoncxx::type::k_dbpointer:
        case bsoncxx::type::k_code:
        case bsoncxx::type::k_symbol:
        case bsoncxx::type::k_timestamp:

            BOOST_LOG_TRIVIAL(fatal) << "valAsInt with type unsuported type in list";
            exit(EXIT_FAILURE);
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "valAsInt with type unsuported type not in list";
            exit(EXIT_FAILURE);
    }
    return (0);
}
double valAsDouble(view_or_value val) {
    auto elem = val.view()[0];
    switch (elem.type()) {
        case bsoncxx::type::k_int64:
            return (static_cast<double>(elem.get_int64().value));
            break;
        case bsoncxx::type::k_int32:
            return (static_cast<double>(elem.get_int32().value));
            break;
        case bsoncxx::type::k_double:
            return (static_cast<double>(elem.get_double().value));
            break;
        case bsoncxx::type::k_utf8:
        case bsoncxx::type::k_document:
        case bsoncxx::type::k_array:
        case bsoncxx::type::k_binary:
        case bsoncxx::type::k_undefined:
        case bsoncxx::type::k_oid:
        case bsoncxx::type::k_bool:
        case bsoncxx::type::k_date:
        case bsoncxx::type::k_null:
        case bsoncxx::type::k_regex:
        case bsoncxx::type::k_dbpointer:
        case bsoncxx::type::k_code:
        case bsoncxx::type::k_symbol:
        case bsoncxx::type::k_timestamp:

            BOOST_LOG_TRIVIAL(fatal) << "valAsInt with type unsuported type in list";
            exit(EXIT_FAILURE);
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "valAsInt with type unsuported type not in list";
            exit(EXIT_FAILURE);
    }
    return (0);
}
}
