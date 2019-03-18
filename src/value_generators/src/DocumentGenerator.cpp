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

#include <cstdlib>
#include <random>
#include <sstream>
#include <unordered_map>
#include <functional>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny {

class IntEvalator {
public:
    virtual ~IntEvalator() = default;
    virtual int64_t evaluate(DefaultRandom& rng) = 0;
};

using UniqueIntEvaluator = std::unique_ptr<IntEvalator>;

UniqueIntEvaluator pickIntEvaluator(YAML::Node node);



class DocEvalator {
public:
    virtual ~DocEvalator() = 0;
    virtual bsoncxx::document::value evaluate(DefaultRandom& rng) = 0;
};

using UniqueDocEvaluator = std::unique_ptr<DocEvalator>;

UniqueDocEvaluator pickDocEvaluator(YAML::Node node);


class UniformIntEvaluator : public IntEvalator {
public:
    UniformIntEvaluator(YAML::Node node)
    : _minGen{pickIntEvaluator(node["min"])}, _maxGen{pickIntEvaluator(node["max"])} {}

    ~UniformIntEvaluator() override = default;

    int64_t evaluate(DefaultRandom &rng) override {
        auto min = _minGen->evaluate(rng);
        auto max = _maxGen->evaluate(rng);
        auto distribution = std::uniform_int_distribution<int64_t>{min, max};
        return distribution(rng);
    }

private:
    UniqueIntEvaluator _minGen;
    UniqueIntEvaluator _maxGen;
};



std::string genString(std::optional<std::string>& alphabetOpt, const std::string& defaultAlphabet, UniqueIntEvaluator& lengthGen, genny::DefaultRandom& rng) {
    auto alphabet = alphabetOpt ? std::string_view{*alphabetOpt} : defaultAlphabet;
    auto alphabetLength = alphabet.size();

    auto distribution = std::uniform_int_distribution<size_t>{0, alphabetLength - 1};

    auto length = lengthGen->evaluate(rng);
    std::string str(length, '\0');

    for (int i = 0; i < length; ++i) {
        str[i] = alphabet[distribution(rng)];
    }

    return str;
}

std::string genFastRandomString(UniqueIntEvaluator& lengthGen, const std::string& alphabet, const size_t alphabetLength, genny::DefaultRandom& rng) {
    auto length = lengthGen->evaluate(rng);
    std::string str(length, '\0');

    auto randomValue = rng();
    int bits = 64;

    for (int i = 0; i < length; ++i) {
        if (bits < 6) {
            randomValue = rng();
            bits = 64;
        }

        str[i] = alphabet[(randomValue & 0x2f) % alphabetLength];
        randomValue >>= 6;
        bits -= 6;
    }
    return str;
}

int64_t genBinomial(genny::DefaultRandom& rng, UniqueIntEvaluator& tGen, double p) {
    auto distribution = std::binomial_distribution<int64_t>{tGen->evaluate(rng), p};
    return distribution(rng);
}

int64_t genNegativeBinomial(genny::DefaultRandom& rng, UniqueIntEvaluator& kGen, double p) {
    auto distribution = std::negative_binomial_distribution<int64_t>{kGen->evaluate(rng), p};
    return distribution(rng);
}

int64_t genGeometric(genny::DefaultRandom& rng, double p) {
    auto distribution = std::geometric_distribution<int64_t>{p};
    return distribution(rng);
}

int64_t genPoisson(DefaultRandom& rng, double mean) {
    auto distribution = std::poisson_distribution<int64_t>{mean};
    return distribution(rng);
}



DocumentGenerator DocumentGenerator::create(YAML::Node node, DefaultRandom& rng) {
    return DocumentGenerator{node, rng};
}

//
//using UniqueIntGen = std::unique_ptr<IntGenerator>;
//using UniqueStrGen = std::unique_ptr<StringGenerator>;
//using UniqueDocGen = std::unique_ptr<DocumentGenerator>;
//using UniqueArrayGen = std::unique_ptr<ArrayGenerator>;

//using Parser = typename std::function<UniqueAppender(YAML::Node, genny::DefaultRandom&)>;

//Parser fastRandomString;
//Parser randomInt;
//Parser randomString;
//Parser verbatim;

//UniqueIntGen createIntGenerator(YAML::Node node, DefaultRandom& rng);
//UniqueStrGen createStringGenerator(YAML::Node node, DefaultRandom& rng);
//UniqueDocGen createDocumentGenerator(YAML::Node node, DefaultRandom& rng);
//UniqueArrayGen createArrayGenerator(YAML::Node node, DefaultRandom& rng);

/*

Negative Binomial:

         UniqueExpression k;
        double p;

        if (auto entry = node["k"]) {
            k = Expression::parseOperand(entry, rng);
        } else {
            throw InvalidValueGeneratorSyntax(
                    "Expected 'k' parameter for negative binomial distribution");
        }

        if (auto entry = node["p"]) {
            p = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax(
                    "Expected 'p' parameter for negative binomial distribution");
        }

        UniqueTypedExpression<IntegerValueType> kTyped =
                std::make_unique<TypedExpression<IntegerValueType>>(std::move(k));

        return std::make_unique<NegativeBinomialIntExpression>(std::move(kTyped), p);

 Geometric:

         double p;

        if (auto entry = node["p"]) {
            p = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'p' parameter for geometric distribution");
        }

        return std::make_unique<GeometricIntExpression>(p);

Poisson:

         double mean;

        if (auto entry = node["mean"]) {
            mean = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'mean' parameter for poisson distribution");
        }

 */
UniqueIntEvaluator pickIntEvaluator(YAML::Node node) {
    auto distribution = node["distribution"].as<std::string>("uniform");

    if (distribution == "uniform") {
        return std::make_unique<UniformIntEvaluator>(node);
//    } else if (distribution == "binomial") {
//        return std::make_unique<BinomialIntGeneratorImpl>(node);
//    } else if (distribution == "negative_binomial") {
//        return std::make_unique<NegativeBinomialIntGeneratorImpl>(node);
//    } else if (distribution == "geometric") {
//        return std::make_unique<GeometricIntGeneratorImpl>(node);
//    } else if (distribution == "poisson") {
//        return std::make_unique<PoissonIntGeneratorImpl>(mean);
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }
}


class IntGenerator::Impl {
private:
    UniqueIntEvaluator _impl;
public:
    Impl(YAML::Node node)
    : _impl{pickIntEvaluator(node)} {}

    int64_t evaluate(DefaultRandom &rng) {
        return _impl->evaluate(rng);
    }
};


class DocumentGenerator::Impl {
private:
    UniqueDocEvaluator _impl;
    Impl(YAML::Node node)
            : _impl{pickDocEvaluator(node)} {}

    bsoncxx::document::value evaluate(DefaultRandom &rng) {
        return _impl->evaluate(rng);
    }
};

// TODO:
DocumentGenerator::DocumentGenerator(YAML::Node node, DefaultRandom &rng) {

}

// TODO
bsoncxx::document::value DocumentGenerator::operator()() {
    return bsoncxx::document::value(nullptr, 0, nullptr);
}

DocumentGenerator::~DocumentGenerator() = default;


//const auto parserMap = std::unordered_map<std::string, Parser>{
//    {"^FastRandomString", fastRandomString},
//    {"^RandomInt", randomInt},
//    {"^RandomString", randomString},
//    {"^Verbatim", verbatim}};


//Value::Value(bool value) : _value(value) {}
//Value::Value(int32_t value) : _value(value) {}
//Value::Value(int64_t value) : _value(value) {}
//Value::Value(double value) : _value(value) {}
//Value::Value(std::string value) : _value(std::move(value)) {}
//Value::Value(bsoncxx::types::b_null value) : _value(std::move(value)) {}
//Value::Value(bsoncxx::document::view_or_value value) : _value(std::move(value)) {}
//Value::Value(bsoncxx::array::view_or_value value) : _value(std::move(value)) {}
//
//bool Value::getBool() const {
//    return std::get<bool>(_value);
//}
//
//int32_t Value::getInt32() const {
//    return std::get<int32_t>(_value);
//}
//
//int64_t Value::getInt64() const {
//    return std::get<int64_t>(_value);
//}
//
//double Value::getDouble() const {
//    return std::get<double>(_value);
//}
//
//std::string Value::getString() const {
//    return std::get<std::string>(_value);
//}
//
//bsoncxx::types::b_null Value::getNull() const {
//    return std::get<bsoncxx::types::b_null>(_value);
//}
//
//bsoncxx::document::view_or_value Value::getDocument() const {
//    return std::get<bsoncxx::document::view_or_value>(_value);
//}
//
//bsoncxx::array::view_or_value Value::getArray() const {
//    return std::get<bsoncxx::array::view_or_value>(_value);
//}
//
//template <class... Ts>
//struct overloaded : Ts... {
//    using Ts::operator()...;
//};
//
//template <class... Ts>
//overloaded(Ts...)->overloaded<Ts...>;
//
//std::optional<int64_t> Value::tryAsInt64() const {
//    std::optional<int64_t> ret;
//
//    std::visit(overloaded{[&](int32_t arg) { ret = arg; },
//                          [&](int64_t arg) { ret = arg; },
//                          [&](auto&& arg) {}},
//               _value);
//
//    return ret;
//}
//
//namespace {
//
//int64_t getInt64Parameter(int64_t value, std::string_view name) {
//    return value;
//}
//
//int64_t getInt64Parameter(const Value& value, std::string_view name) {
//    auto ret = value.tryAsInt64();
//
//    if (!ret) {
//        std::stringstream error;
//        error << "Expected integer for parameter '" << name << "', but got " << value;
//        throw InvalidValueGeneratorSyntax(error.str());
//    }
//
//    return *ret;
//}
//
//}  // namespace
//
//void Value::appendToBuilder(bsoncxx::builder::basic::document& doc, std::string key) {
//    std::visit([&](auto&& arg) { doc.append(bsoncxx::builder::basic::kvp(std::move(key), arg)); },
//               std::move(_value));
//}
//
//void Value::appendToBuilder(bsoncxx::builder::basic::array& arr) {
//    std::visit([&](auto&& arg) { arr.append(arg); }, std::move(_value));
//}
//
//std::ostream& operator<<(std::ostream& out, const Value& value) {
//    std::visit(overloaded{
//                   [&](bool arg) { out << arg; },
//                   [&](int32_t arg) { out << arg; },
//                   [&](int64_t arg) { out << arg; },
//                   [&](double arg) { out << arg; },
//                   [&](const std::string& arg) { out << arg; },
//                   [&](bsoncxx::types::b_null arg) { out << "null"; },
//                   [&](const bsoncxx::document::value& arg) {
//                       out << bsoncxx::to_json(arg.view(), bsoncxx::ExtendedJsonMode::k_canonical);
//                   },
//                   [&](bsoncxx::document::view arg) {
//                       out << bsoncxx::to_json(arg, bsoncxx::ExtendedJsonMode::k_canonical);
//                   },
//                   [&](const bsoncxx::array::value& arg) {
//                       out << bsoncxx::to_json(arg.view(), bsoncxx::ExtendedJsonMode::k_canonical);
//                   },
//                   [&](bsoncxx::array::view arg) {
//                       out << bsoncxx::to_json(arg, bsoncxx::ExtendedJsonMode::k_canonical);
//                   },
//               },
//               value._value);
//
//    return out;
//}

//std::map<std::string, Parser> parserMap {};

//UniqueAppender createDocGenerator(YAML::Node node, DefaultRandom& rng) {
//    if (!node.IsMap()) {
//        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an expression");
//    }
//
//    auto nodeIt = node.begin();
//    if (nodeIt == node.end()) {
//        throw InvalidValueGeneratorSyntax(
//            "Expected mapping to have a single '^'-prefixed key, but was empty");
//    }
//
//    auto key = nodeIt->first;
//    auto value = nodeIt->second;
//
//    ++nodeIt;
//    if (nodeIt != node.end()) {
//        throw InvalidValueGeneratorSyntax(
//            "Expected mapping to have a single '^'-prefixed key, but had multiple keys");
//    }
//
//    auto name = key.as<std::string>();
//
//    auto parserIt = parserMap.find(name);
//    if (parserIt == parserMap.end()) {
//        std::stringstream error;
//        error << "Unknown expression type '" << name << "'";
//        throw InvalidValueGeneratorSyntax(error.str());
//    }
//    return parserIt->second(value, rng);
//}

//UniqueExpression Expression::parseObject(YAML::Node node, DefaultRandom& rng) {
//    if (!node.IsMap()) {
//        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
//    }
//
//    auto nodeIt = node.begin();
//    if (nodeIt != node.end()) {
//        auto key = nodeIt->first;
//
//        ++nodeIt;
//        if (nodeIt == node.end() && key.as<std::string>()[0] == '^') {
//            return Expression::parseExpression(node, rng);
//        }
//    }
//
//    return DocumentExpression::parse(node, rng);
//}

//UniqueExpression Expression::parseOperand(YAML::Node node, DefaultRandom& rng) {
//    switch (node.Type()) {
//        case YAML::NodeType::Map:
//            return Expression::parseObject(node, rng);
//        case YAML::NodeType::Sequence:
//            return ArrayExpression::parse(node, rng);
//        case YAML::NodeType::Scalar:
//        case YAML::NodeType::Null:
//            return ConstantExpression::parse(node, rng);
//        case YAML::NodeType::Undefined:
//            throw InvalidValueGeneratorSyntax(
//                "C++ programmer error: Failed to check for node's existence before attempting to"
//                " parse it");
//    }
//
//    std::abort();
//}
//
//ConstantExpression::ConstantExpression(Value value, ValueType type)
//    : _type{type}, _value{std::move(value)} {}
//
//UniqueAppender parse(YAML::Node node, DefaultRandom& rng) {
//    switch (node.Type()) {
//        case YAML::NodeType::Map: {
//            auto elements = std::vector<DocumentExpression::ElementType>{};
//            for (auto&& entry : node) {
//                elements.emplace_back(entry.first.as<std::string>(),
//                                      ConstantExpression::parse(entry.second, rng));
//            }
//
//            return std::make_unique<DocumentExpression>(std::move(elements));
//        }
//        case YAML::NodeType::Sequence: {
//            auto elements = std::vector<ArrayExpression::ElementType>{};
//            for (auto&& entry : node) {
//                elements.emplace_back(ConstantExpression::parse(entry, rng));
//            }
//
//            return std::make_unique<ArrayExpression>(std::move(elements));
//        }
//        case YAML::NodeType::Null:
//            return std::make_unique<ConstantExpression>(Value{bsoncxx::types::b_null{}},
//                                                        ValueType::Null);
//        case YAML::NodeType::Scalar:
//        case YAML::NodeType::Undefined:
//            // YAML::NodeType::Scalar and YAML::NodeType::Undefined are handled below.
//            break;
//    }
//
//    if (!node.IsScalar()) {
//        throw InvalidValueGeneratorSyntax("Expected scalar value to parse");
//    }
//
//    // `YAML::Node::Tag() == "!"` means that the scalar value is quoted. In that case, we want to
//    // avoid converting any numeric string values to numbers. See
//    // https://github.com/jbeder/yaml-cpp/issues/261 for more details.
//    if (node.Tag() != "!") {
//        try {
//            return std::make_unique<ConstantExpression>(Value{node.as<int32_t>()},
//                                                        ValueType::Integer);
//        } catch (const YAML::BadConversion& e) {
//        }
//
//        try {
//            return std::make_unique<ConstantExpression>(Value{node.as<int64_t>()},
//                                                        ValueType::Integer);
//        } catch (const YAML::BadConversion& e) {
//        }
//
//        try {
//            return std::make_unique<ConstantExpression>(Value{node.as<double>()},
//                                                        ValueType::Double);
//        } catch (const YAML::BadConversion& e) {
//        }
//
//        try {
//            return std::make_unique<ConstantExpression>(Value{node.as<bool>()}, ValueType::Boolean);
//        } catch (const YAML::BadConversion& e) {
//        }
//    }
//
//    return std::make_unique<ConstantExpression>(Value{node.as<std::string>()}, ValueType::String);
//}
//
//Value ConstantExpression::evaluate(genny::DefaultRandom& rng) const {
//    return _value;
//}
//
//DocumentExpression::DocumentExpression(std::vector<ElementType> elements)
//    : _elements(std::move(elements)) {}
//
//UniqueExpression DocumentExpression::parse(YAML::Node node, DefaultRandom& rng) {
//    if (!node.IsMap()) {
//        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
//    }
//
//    auto elements = std::vector<ElementType>{};
//    for (auto&& entry : node) {
//        if (entry.first.as<std::string>()[0] == '^') {
//            throw InvalidValueGeneratorSyntax(
//                "'^'-prefix keys are reserved for expressions, but attempted to parse as an"
//                " object");
//        }
//
//        elements.emplace_back(entry.first.as<std::string>(),
//                              Expression::parseOperand(entry.second, rng));
//    }
//
//    return std::make_unique<DocumentExpression>(std::move(elements));
//}
//
//Value DocumentExpression::evaluate(genny::DefaultRandom& rng) const {
//    auto doc = bsoncxx::builder::basic::document{};
//
//    for (auto&& elem : _elements) {
//        elem.second->evaluate(rng).appendToBuilder(doc, elem.first);
//    }
//
//    return Value{doc.extract()};
//}
//
//ArrayExpression::ArrayExpression(std::vector<ElementType> elements)
//    : _elements(std::move(elements)) {}
//
//UniqueExpression ArrayExpression::parse(YAML::Node node, DefaultRandom& rng) {
//    if (!node.IsSequence()) {
//        throw InvalidValueGeneratorSyntax("Expected sequence type to parse into an array");
//    }
//
//    auto elements = std::vector<ElementType>{};
//    for (auto&& entry : node) {
//        elements.emplace_back(Expression::parseOperand(entry, rng));
//    }
//
//    return std::make_unique<ArrayExpression>(std::move(elements));
//}
//
//Value ArrayExpression::evaluate(genny::DefaultRandom& rng) const {
//    auto arr = bsoncxx::builder::basic::array{};
//
//    for (auto&& elem : _elements) {
//        elem->evaluate(rng).appendToBuilder(arr);
//    }
//
//    return Value{arr.extract()};
//}
//
//
//
//UniqueIntGen uniformIntGen(YAML::Node node, DefaultRandom& rng) {
//}
//
//UniqueIntGen binomialGen(YAML::Node node, DefaultRandom& rng) {
//    UniqueExpression t;
//    double p;
//
//    if (auto entry = node["t"]) {
//        t = Expression::parseOperand(entry, rng);
//    } else {
//        throw InvalidValueGeneratorSyntax("Expected 't' parameter for binomial distribution");
//    }
//
//    if (auto entry = node["p"]) {
//        p = entry.as<double>();
//    } else {
//        throw InvalidValueGeneratorSyntax("Expected 'p' parameter for binomial distribution");
//    }
//
//    return std::make_unique<BinomialIntExpression>(std::move(tTyped), p);
//}

//
//UniformIntExpression::UniformIntExpression(UniqueTypedExpression<IntegerValueType> min,
//                                           UniqueTypedExpression<IntegerValueType> max)
//    : _min(std::move(min)), _max(std::move(max)) {}
//
//Value UniformIntExpression::evaluate(genny::DefaultRandom& rng) const {
//    auto min = getInt64Parameter(_min->evaluate(rng), "min");
//    auto max = getInt64Parameter(_max->evaluate(rng), "max");
//
//    auto distribution = std::uniform_int_distribution<int64_t>{min, max};
//    return Value{distribution(rng)};
//}
//
//
//UniqueExpression RandomStringExpression::parse(YAML::Node node, DefaultRandom& rng) {
//    UniqueExpression length;
//    std::optional<std::string> alphabet;
//
//    if (auto entry = node["length"]) {
//        length = Expression::parseOperand(entry, rng);
//    } else {
//        throw InvalidValueGeneratorSyntax(
//            "Expected 'length' parameter for random string generator");
//    }
//
//    if (auto entry = node["alphabet"]) {
//        alphabet = entry.as<std::string>();
//
//        if (alphabet->empty()) {
//            throw InvalidValueGeneratorSyntax(
//                "Expected non-empty 'alphabet' parameter for random string generator");
//        }
//    }
//
//    UniqueTypedExpression<IntegerValueType> lengthT =
//        std::make_unique<TypedExpression<IntegerValueType>>(std::move(length));
//
//    return std::make_unique<RandomStringExpression>(std::move(lengthT), std::move(alphabet));
//}
//
//
//UniqueExpression FastRandomStringExpression::parse(YAML::Node node, DefaultRandom& rng) {
//    UniqueExpression length;
//
//    if (auto entry = node["length"]) {
//        length = Expression::parseOperand(entry, rng);
//    } else {
//        throw InvalidValueGeneratorSyntax("Expected 'length' parameter for fast random string");
//    }
//
//    UniqueTypedExpression<IntegerValueType> lengthT =
//        std::make_unique<TypedExpression<IntegerValueType>>(std::move(length));
//    return std::make_unique<FastRandomStringExpression>(std::move(lengthT));
//}
//

}  // namespace genny
