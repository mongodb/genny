// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generators-private.hh"

#include <string_view>
#include <unordered_set>

#include <bsoncxx/types/value.hpp>

#include <boost/log/trivial.hpp>

namespace genny::value_generators {

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace {

constexpr std::string_view kRandomIntType = "^RandomInt";
constexpr std::string_view kRandomStringType = "^RandomString";
constexpr std::string_view kFastRandomStringType = "^FastRandomString";
constexpr std::string_view kUseValueType = "^UseValue";
const std::unordered_set<std::string_view> kGeneratorTypes{
    kFastRandomStringType,
    kRandomIntType,
    kRandomStringType,
    kUseValueType,
};

}  // namespace

BsonDocument::BsonDocument()
    : doc(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize) {}

BsonDocument::BsonDocument(const YAML::Node node)
    : DocumentGenerator(), doc(parser::parseMap(node)) {}

bsoncxx::document::view BsonDocument::view(bsoncxx::builder::stream::document&) {
    return doc->view();
}

TemplateDocument::TemplateDocument(YAML::Node node, genny::DefaultRandom& rng)
    : DocumentGenerator() {
    std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;

    BOOST_LOG_TRIVIAL(trace) << "In TemplateDocument constructor";
    doc.setDoc(parser::parseMap(node, kGeneratorTypes, "", overrides));
    BOOST_LOG_TRIVIAL(trace)
        << "In TemplateDocument constructor. Parsed the document. About to deal with overrides";
    for (auto entry : overrides) {
        auto key = std::get<0>(entry);
        auto type = std::get<1>(entry);
        YAML::Node yamlOverride = std::get<2>(entry);
        BOOST_LOG_TRIVIAL(trace) << "In TemplateDocument constructor. Dealing with an override for "
                                 << key;

        BOOST_LOG_TRIVIAL(trace) << "Making value generator for key " << key << " and type "
                                 << type;
        override[key] = makeUniqueValueGenerator(yamlOverride, type, rng);
    }
}

void TemplateDocument::applyOverrideLevel(bsoncxx::builder::stream::document& output,
                                          bsoncxx::document::view doc,
                                          std::string prefix) {
    // Going to need variants of this for arrays

    // iterate through keys. if key matches exactly, replace in output.
    // if key doesn't match, copy element to output
    // if key prefix matches, descend a level.

    // process override for elements at this level

    // I don't think I want this as a naked pointer. It's owned
    // above. Can switch to shared_ptr
    std::unordered_map<std::string, ValueGenerator*> thislevel;
    // process override for elements at lower level
    std::set<std::string> lowerlevel;
    //    cout << "prefix is " << prefix ;
    for (auto& elem : override) {
        std::string key = elem.first;
        //        BOOST_LOG_TRIVIAL(trace) << "Going through overrides key: " << key << " value
        //        is "
        //                         << elem.second << " prefix.length() = " << prefix.length();
        if (prefix == "" || key.compare(0, prefix.length(), prefix) == 0) {
            // prefix match. Need what comes after
            // grab everything after prefix
            // BOOST_LOG_TRIVIAL(trace) << "Key matched with prefix";
            auto suffix = key.substr(prefix.length(), key.length() - prefix.length());
            // check for a period. If no period, put in thislevel
            auto find = suffix.find('.');
            // no match
            if (find == std::string::npos) {
                thislevel[suffix] = elem.second.get();
                //  BOOST_LOG_TRIVIAL(trace) << "Putting thislevel[" << suffix << "]=" <<
                //  elem.second;
            } else {
                // if period, grab from suffix to period and put in lowerlevel
                // We won't actually use the second element here
                // BOOST_LOG_TRIVIAL(trace) << "Putting lowerlevel[" << suffix << "]=" <<
                // elem.second;
                lowerlevel.insert(suffix.substr(0, find));
            }
        } else {
            // BOOST_LOG_TRIVIAL(trace) << "No prefix match";
        }
    }

    for (auto elem : doc) {
        // BOOST_LOG_TRIVIAL(trace) << "Looking at key " << elem.key().to_string();
        auto iter = thislevel.find(std::string{elem.key()});
        auto iter2 = lowerlevel.find(std::string{elem.key()});
        if (iter != thislevel.end()) {
            // replace this entry
            // BOOST_LOG_TRIVIAL(trace) << "Matched on this level. Replacing ";
            output << std::string{elem.key()} << iter->second->generate().view()[0].get_value();
        } else if (iter2 != lowerlevel.end()) {
            // need to check if child is document, array, or other.
            //            BOOST_LOG_TRIVIAL(trace) << "Partial match. Need to descend";
            switch (elem.type()) {
                case bsoncxx::type::k_document: {
                    bsoncxx::builder::stream::document mydoc{};
                    applyOverrideLevel(
                        mydoc, elem.get_document().value, prefix + std::string{elem.key()} + '.');
                    output << std::string{elem.key()} << open_document
                           << bsoncxx::builder::concatenate(mydoc.view()) << close_document;
                } break;
                case bsoncxx::type::k_array:
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Trying to descend a level of bson in overrides. Array not "
                           "supported "
                           "yet.";
                default:
                    throw InvalidConfigurationException(
                        "Trying to descend a level of bson in "
                        "overrides but not a map or "
                        "array");
            }
        } else {
            //            BOOST_LOG_TRIVIAL(trace) << "No match, just pass through";
            bsoncxx::types::value ele_val{elem.get_value()};
            output << std::string{elem.key()} << ele_val;
        }
    }
}

bsoncxx::document::view TemplateDocument::view(bsoncxx::builder::stream::document& output) {
    // Need to iterate through the doc, and for any field see if it
    // matches. Override the value if it does.
    // bson output

    // scope problem -- output is going out of scope here
    // to be thread safe this has to be on the stack or in the per thread data.

    // Not sure I need the tempdoc in addition to output
    bsoncxx::builder::stream::document tempdoc{};
    applyOverrideLevel(output, doc.view(tempdoc), "");
    return output.view();
}
ValueGenerator* makeValueGenerator(YAML::Node yamlNode,
                                   std::string type,
                                   genny::DefaultRandom& rng) {
    if (type == kRandomIntType) {
        return new RandomIntGenerator(yamlNode, rng);
    } else if (type == kRandomStringType) {
        return new RandomStringGenerator(yamlNode, rng);
    } else if (type == kFastRandomStringType) {
        return new FastRandomStringGenerator(yamlNode, rng);
    } else if (type == kUseValueType) {
        return new UseValueGenerator(yamlNode, rng);
    }
    std::stringstream error;
    error << "In makeValueGenerator and don't know how to handle type " << type;
    throw InvalidConfigurationException(error.str());
}

ValueGenerator* makeValueGenerator(YAML::Node yamlNode, genny::DefaultRandom& rng) {
    if (auto type = yamlNode["type"])
        return (makeValueGenerator(yamlNode, type.Scalar(), rng));
    // If it doesn't have a type field, search for templating keys
    for (auto&& entry : yamlNode) {
        auto key = entry.first.Scalar();
        if (kGeneratorTypes.count(key)) {
            return (makeValueGenerator(entry.second, std::move(key), rng));
        }
    }
    return (makeValueGenerator(yamlNode, std::string{kUseValueType}, rng));
}

int64_t ValueGenerator::generateInt() {
    return valAsInt(generate());
};
double ValueGenerator::generateDouble() {
    return valAsDouble(generate());
};
std::string ValueGenerator::generateString() {
    return valAsString(generate());
}


std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node yamlNode,
                                                         genny::DefaultRandom& rng) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode, rng));
}
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node yamlNode,
                                                         std::string type,
                                                         genny::DefaultRandom& rng) {
    return std::unique_ptr<ValueGenerator>(makeValueGenerator(yamlNode, type, rng));
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
            return (std::string{elem.get_utf8().value});
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

            throw InvalidConfigurationException("valAsString with type unsuported type in list");
            break;
        default:
            throw InvalidConfigurationException(
                "valAsString with type unsuported type not in list");
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

            throw InvalidConfigurationException("valAsInt with type unsuported type in list");
            break;
        default:
            throw InvalidConfigurationException("valAsInt with type unsuported type not in list");
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

            throw InvalidConfigurationException("valAsInt with type unsuported type in list");
            break;
        default:
            throw InvalidConfigurationException("valAsInt with type unsuported type not in list");
    }
    return (0);
}

UseValueGenerator::UseValueGenerator(YAML::Node& node, genny::DefaultRandom& rng)
    : ValueGenerator(node, rng) {
    // add in error checking
    if (node.IsScalar()) {
        value = parser::yamlToValue(node);
    } else {
        value = parser::yamlToValue(node["value"]);
    }
}

bsoncxx::array::value UseValueGenerator::generate() {
    // probably should actually return a view or a copy of the value
    return (bsoncxx::array::value(*value));
}

RandomIntGenerator::RandomIntGenerator(const YAML::Node& node, genny::DefaultRandom& rng)
    : ValueGenerator(node, rng), generator(GeneratorType::UNIFORM), min(0), max(100), t(10) {
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
                std::stringstream error;
                error << "In RandomIntGenerator and have unknown distribution type "
                      << distributionString;
                throw InvalidConfigurationException(error.str());
            }
        }
        // now read in parameters based on the distribution type
        switch (generator) {
            case GeneratorType::UNIFORM:
                if (auto minimum = node["min"]) {
                    min = IntOrValue(minimum, rng);
                }
                if (auto maximum = node["max"]) {
                    max = IntOrValue(maximum, rng);
                }
                break;
            case GeneratorType::BINOMIAL:
                if (auto trials = node["t"])
                    t = IntOrValue(trials, rng);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Binomial distribution in random int, but no t parameter";
                if (auto probability = node["p"])
                    p = probability.as<double>();
                else {
                    throw InvalidConfigurationException(
                        "Binomial distribution in random int, but no p parameter");
                }
                break;
            case GeneratorType::NEGATIVE_BINOMIAL:
                if (auto kval = node["k"])
                    t = IntOrValue(kval, rng);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Negative binomial distribution in random int, not no k parameter";
                if (auto probability = node["p"])
                    p = probability.as<double>();
                else {
                    throw InvalidConfigurationException(
                        "Binomial distribution in random int, but no p parameter");
                }
                break;
            case GeneratorType::GEOMETRIC:
                if (auto probability = node["p"])
                    p = probability.as<double>();
                else {
                    throw InvalidConfigurationException(
                        "Geometric distribution in random int, but no p parameter");
                }
                break;
            case GeneratorType::POISSON:
                if (auto meannode = node["mean"])
                    mean = meannode.as<double>();
                else {
                    throw InvalidConfigurationException(
                        "Geometric distribution in random int, but no p parameter");
                }
                break;
            default:
                throw InvalidConfigurationException(
                    "Unknown generator type in RandomIntGenerator in switch statement");
                break;
        }
    }
}

int64_t RandomIntGenerator::generateInt() {
    switch (generator) {
        case GeneratorType::UNIFORM: {
            std::uniform_int_distribution<int64_t> distribution(min.getInt(), max.getInt());
            return (distribution(_rng));
        } break;
        case GeneratorType::BINOMIAL: {
            std::binomial_distribution<int64_t> distribution(t.getInt(), p);
            return (distribution(_rng));
        } break;
        case GeneratorType::NEGATIVE_BINOMIAL: {
            std::negative_binomial_distribution<int64_t> distribution(t.getInt(), p);
            return (distribution(_rng));
        } break;
        case GeneratorType::GEOMETRIC: {
            std::geometric_distribution<int64_t> distribution(p);
            return (distribution(_rng));
        } break;
        case GeneratorType::POISSON: {
            std::poisson_distribution<int64_t> distribution(mean);
            return (distribution(_rng));
        } break;
        default:
            throw InvalidConfigurationException(
                "Unknown generator type in RandomIntGenerator in switch statement");
            break;
    }
    throw InvalidConfigurationException(
        "Reached end of RandomIntGenerator::generateInt. Should have returned earlier");
}
std::string RandomIntGenerator::generateString() {
    return (std::to_string(generateInt()));
}
bsoncxx::array::value RandomIntGenerator::generate() {
    return (bsoncxx::builder::stream::array{} << generateInt()
                                              << bsoncxx::builder::stream::finalize);
}

IntOrValue::IntOrValue(YAML::Node yamlNode, genny::DefaultRandom& rng)
    : myInt(0), myGenerator(nullptr) {
    if (yamlNode.IsScalar()) {
        // Read in just a number
        isInt = true;
        myInt = yamlNode.as<int64_t>();
    } else {
        // use a value generator
        isInt = false;
        myGenerator = makeUniqueValueGenerator(yamlNode, rng);
    }
}
FastRandomStringGenerator::FastRandomStringGenerator(const YAML::Node& node,
                                                     genny::DefaultRandom& rng)
    : ValueGenerator(node, rng) {
    if (node["length"]) {
        length = IntOrValue(node["length"], rng);
    } else {
        length = IntOrValue(10);
    }
}
bsoncxx::array::value FastRandomStringGenerator::generate() {
    std::string str;
    auto thisLength = length.getInt();
    str.resize(thisLength);
    auto randomnum = _rng();
    int bits = 64;
    for (int i = 0; i < thisLength; i++) {
        if (bits < 6) {
            bits = 64;
            randomnum = _rng();
        }
        str[i] = fastAlphaNum[(randomnum & 0x2f) % fastAlphaNumLength];
        randomnum >>= 6;
        bits -= 6;
    }
    return (bsoncxx::builder::stream::array{} << str << bsoncxx::builder::stream::finalize);
}

RandomStringGenerator::RandomStringGenerator(YAML::Node& node, genny::DefaultRandom& rng)
    : ValueGenerator(node, rng) {
    if (node["length"]) {
        length = IntOrValue(node["length"], rng);
    } else {
        length = IntOrValue(10);
    }
    if (node["alphabet"]) {
        alphabet = node["alphabet"].Scalar();
    } else {
        alphabet = alphaNum;
    }
}
bsoncxx::array::value RandomStringGenerator::generate() {
    std::string str;
    auto alphabetLength = alphabet.size();
    std::uniform_int_distribution<int> distribution(0, alphabetLength - 1);
    auto thisLength = length.getInt();
    str.resize(thisLength);
    for (int i = 0; i < thisLength; i++) {
        str[i] = alphabet[distribution(_rng)];
    }
    return (bsoncxx::builder::stream::array{} << str << bsoncxx::builder::stream::finalize);
}
}  // namespace genny::value_generators
