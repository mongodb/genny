#include <gennylib/generators.hpp>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <random>
#include <stdlib.h>

using bsoncxx::builder::stream::finalize;

namespace genny {

bsonDocument::bsonDocument() {
    doc = bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize;
}

bsonDocument::bsonDocument(const YAML::Node node) {
    if (!node) {
        BOOST_LOG_TRIVIAL(info) << "bsonDocument constructor using empty document";
    } else if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in bsonDocument constructor";
        exit(EXIT_FAILURE);
    } else {
        BOOST_LOG_TRIVIAL(trace) << "In bsonDocument constructor";
        doc = parseMap(node);
        BOOST_LOG_TRIVIAL(trace) << "Parsed map in bsonDocument constructor";
    }
}

bsoncxx::document::view bsonDocument::view(bsoncxx::builder::stream::document&, std::mt19937_64&) {
    return doc->view();
}

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(const YAML::Node node) {
    if (!node) {  // empty document should be bsonDocument
        return unique_ptr<document>{new bsonDocument(node)};
    } else  // if (!node["type"] or node["type"].Scalar() == "bson") {
        return unique_ptr<document>{new bsonDocument(node)};
    // } else if (!node["type"] or node["type"].Scalar() == "templating") {
    //   return unique_ptr<document>{new templateDocument(node)};
    // } else if (node["type"].Scalar() == "bson") {
    //   return unique_ptr<document>{new bsonDocument(node)};
    // } else if (node["type"].Scalar() == "override") {
    //   return unique_ptr<document>{new overrideDocument(node)};
    // } else if (node["type"].Scalar() == "append") {
    //   return unique_ptr<document>{new AppendDocument(node)};
    // } else {
    //   BOOST_LOG_TRIVIAL(fatal)
    //       << "in makeDoc and type exists, and isn't bson or override";
    //   exit(EXIT_FAILURE);
    // }
};

// This returns a set of the value generator types with $ prefixes
const std::set<std::string> getGeneratorTypes() {
    return (std::set<std::string>{
        // "$add",
        // "$choose",
        // "$concatenate",
        // "$date",
        // "$increment",
        // "$multiply",
        "$randomint",
        // "$fastrandomstring",
        // "$randomstring",
        // "$useresult",
        "$useval",
        // "$usevar"});
    });
}

ValueGenerator* makeValueGenerator(YAML::Node yamlNode, std::string type) {
    // if (type == "add") {
    //     return new AddGenerator(yamlNode);
    // } else if (type == "choose") {
    //     return new ChooseGenerator(yamlNode);
    // } else if (type == "concatenate") {
    //     return new ConcatenateGenerator(yamlNode);
    // } else if (type == "date") {
    //     return new DateGenerator(yamlNode);
    // } else if (type == "increment") {
    //     return new IncrementGenerator(yamlNode);
    // } else if (type == "multiply") {
    //     return new MultiplyGenerator(yamlNode);
    // } else
    if (type == "randomint") {
        return new RandomIntGenerator(yamlNode);
    }  // else if (type == "randomstring") {
    //     return new RandomStringGenerator(yamlNode);
    // } else if (type == "fastrandomstring") {
    //     return new FastRandomStringGenerator(yamlNode);
    // } else if (type == "useresult") {
    //     return new UseResultGenerator(yamlNode);
    else if (type == "useval") {
        return new UseValueGenerator(yamlNode);
    }
    // else if (type == "usevar") {
    //     return new UseVarGenerator(yamlNode);
    // }
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

int64_t ValueGenerator::generateInt(std::mt19937_64& state) {
    return valAsInt(generate(state));
};
double ValueGenerator::generateDouble(std::mt19937_64& state) {
    return valAsDouble(generate(state));
};
std::string ValueGenerator::generateString(std::mt19937_64& state) {
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

UseValueGenerator::UseValueGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    if (node.IsScalar()) {
        value = yamlToValue(node);
    } else {
        value = yamlToValue(node["value"]);
    }
}

bsoncxx::array::value UseValueGenerator::generate(std::mt19937_64& state) {
    // probably should actually return a view or a copy of the value
    return (bsoncxx::array::value(*value));
}
RandomIntGenerator::RandomIntGenerator(const YAML::Node& node)
    : ValueGenerator(node), generator(GeneratorType::UNIFORM), min(0), max(100), t(10) {
    // It's okay to have a scalar for the templating. Just use defaults
    if (node.IsMap()) {
        if (auto distributionNode = node["distribution"]) {
            auto distributionString = distributionNode.Scalar();
            if (distributionString == "uniform")
                generator = GeneratorType::UNIFORM;
            else if (distributionString == "binomial")
                generator = GeneratorType::BINOMIAL;
            else if (distributionString == "negative_binomial")
                generator = GeneratorType::NEGATIVE_BINOMIAL;
            else if (distributionString == "geometric")
                generator = GeneratorType::GEOMETRIC;
            else if (distributionString == "poisson")
                generator = GeneratorType::POISSON;
            else {
                BOOST_LOG_TRIVIAL(fatal)
                    << "In RandomIntGenerator and have unknown distribution type "
                    << distributionString;
                exit(EXIT_FAILURE);
            }
        }
        // now read in parameters based on the distribution type
        switch (generator) {
            case GeneratorType::UNIFORM:
                if (auto minimum = node["min"]) {
                    min = IntOrValue(minimum);
                }
                if (auto maximum = node["max"]) {
                    max = IntOrValue(maximum);
                }
                break;
            case GeneratorType::BINOMIAL:
                if (auto trials = node["t"])
                    t = IntOrValue(trials);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Binomial distribution in random int, but no t parameter";
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Binomial distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::NEGATIVE_BINOMIAL:
                if (auto kval = node["k"])
                    t = IntOrValue(kval);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Negative binomial distribution in random int, not no k parameter";
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Binomial distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::GEOMETRIC:
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Geometric distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::POISSON:
                if (auto meannode = node["mean"])
                    mean = makeUniqueValueGenerator(meannode);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Geometric distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                BOOST_LOG_TRIVIAL(fatal)
                    << "Unknown generator type in RandomIntGenerator in switch statement";
                exit(EXIT_FAILURE);
                break;
        }
    }
}

int64_t RandomIntGenerator::generateInt(std::mt19937_64& rng) {
    switch (generator) {
        case GeneratorType::UNIFORM: {
            uniform_int_distribution<int64_t> distribution(min.getInt(rng), max.getInt(rng));
            return (distribution(rng));
        } break;
        case GeneratorType::BINOMIAL: {
            binomial_distribution<int64_t> distribution(t.getInt(rng), p->generateDouble(rng));
            return (distribution(rng));
        } break;
        case GeneratorType::NEGATIVE_BINOMIAL: {
            negative_binomial_distribution<int64_t> distribution(t.getInt(rng),
                                                                 p->generateDouble(rng));
            return (distribution(rng));
        } break;
        case GeneratorType::GEOMETRIC: {
            geometric_distribution<int64_t> distribution(p->generateDouble(rng));
            return (distribution(rng));
        } break;
        case GeneratorType::POISSON: {
            poisson_distribution<int64_t> distribution(mean->generateDouble(rng));
            return (distribution(rng));
        } break;
        default:
            BOOST_LOG_TRIVIAL(fatal)
                << "Unknown generator type in RandomIntGenerator in switch rngment";
            exit(EXIT_FAILURE);
            break;
    }
    BOOST_LOG_TRIVIAL(fatal)
        << "Reached end of RandomIntGenerator::generateInt. Should have returned earlier";
    exit(EXIT_FAILURE);
}
std::string RandomIntGenerator::generateString(std::mt19937_64& rng) {
    return (std::to_string(generateInt(rng)));
}
bsoncxx::array::value RandomIntGenerator::generate(std::mt19937_64& rng) {
    return (bsoncxx::builder::stream::array{} << generateInt(rng)
                                              << bsoncxx::builder::stream::finalize);
}

IntOrValue::IntOrValue(YAML::Node yamlNode) : myInt(0), myGenerator(nullptr) {
    if (yamlNode.IsScalar()) {
        // Read in just a number
        isInt = true;
        myInt = yamlNode.as<int64_t>();
    } else {
        // use a value generator
        isInt = false;
        myGenerator = makeUniqueValueGenerator(yamlNode);
    }
}
}  // namespace genny
