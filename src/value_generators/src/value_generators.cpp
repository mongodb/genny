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

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <value_generators/value_generators.hpp>

namespace genny::value_generators::v1 {
namespace {

const auto parserMap = std::unordered_map<std::string, Expression::Parser>{
    {"^FastRandomString", FastRandomStringExpression::parse},
    {"^RandomInt", RandomIntExpression::parse},
    {"^RandomString", RandomStringExpression::parse},
    {"^Verbatim", ConstantExpression::parse}};

}  // namespace

Value::Value(bool value) : _value(value) {}
Value::Value(int32_t value) : _value(value) {}
Value::Value(int64_t value) : _value(value) {}
Value::Value(double value) : _value(value) {}
Value::Value(std::string value) : _value(std::move(value)) {}
Value::Value(bsoncxx::types::b_null value) : _value(std::move(value)) {}
Value::Value(bsoncxx::document::view_or_value value) : _value(std::move(value)) {}
Value::Value(bsoncxx::array::view_or_value value) : _value(std::move(value)) {}

bool Value::getBool() const {
    return std::get<bool>(_value);
}

int32_t Value::getInt32() const {
    return std::get<int32_t>(_value);
}

int64_t Value::getInt64() const {
    return std::get<int64_t>(_value);
}

double Value::getDouble() const {
    return std::get<double>(_value);
}

std::string Value::getString() const {
    return std::get<std::string>(_value);
}

bsoncxx::types::b_null Value::getNull() const {
    return std::get<bsoncxx::types::b_null>(_value);
}

bsoncxx::document::view_or_value Value::getDocument() const {
    return std::get<bsoncxx::document::view_or_value>(_value);
}

bsoncxx::array::view_or_value Value::getArray() const {
    return std::get<bsoncxx::array::view_or_value>(_value);
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;

std::optional<int64_t> Value::tryAsInt64() const {
    std::optional<int64_t> ret;

    std::visit(overloaded{[&](int32_t arg) { ret = arg; },
                          [&](int64_t arg) { ret = arg; },
                          [&](auto&& arg) {}},
               _value);

    return ret;
}

namespace {

int64_t getInt64Parameter(long long value, std::string_view name) {
    return value;
}

int64_t getInt64Parameter(const Value& value, std::string_view name) {
    auto ret = value.tryAsInt64();

    if (!ret) {
        std::stringstream error;
        error << "Expected integer for parameter '" << name << "', but got " << value;
        throw InvalidValueGeneratorSyntax(error.str());
    }

    return *ret;
}

}  // namespace

void Value::appendToBuilder(bsoncxx::builder::basic::document& doc, std::string key) {
    std::visit([&](auto&& arg) { doc.append(bsoncxx::builder::basic::kvp(std::move(key), arg)); },
               std::move(_value));
}

void Value::appendToBuilder(bsoncxx::builder::basic::array& arr) {
    std::visit([&](auto&& arg) { arr.append(arg); }, std::move(_value));
}

std::ostream& operator<<(std::ostream& out, const Value& value) {
    std::visit(overloaded{
                   [&](bool arg) { out << arg; },
                   [&](int32_t arg) { out << arg; },
                   [&](int64_t arg) { out << arg; },
                   [&](double arg) { out << arg; },
                   [&](const std::string& arg) { out << arg; },
                   [&](bsoncxx::types::b_null arg) { out << "null"; },
                   [&](const bsoncxx::document::value& arg) {
                       out << bsoncxx::to_json(arg.view(), bsoncxx::ExtendedJsonMode::k_canonical);
                   },
                   [&](bsoncxx::document::view arg) {
                       out << bsoncxx::to_json(arg, bsoncxx::ExtendedJsonMode::k_canonical);
                   },
                   [&](const bsoncxx::array::value& arg) {
                       out << bsoncxx::to_json(arg.view(), bsoncxx::ExtendedJsonMode::k_canonical);
                   },
                   [&](bsoncxx::array::view arg) {
                       out << bsoncxx::to_json(arg, bsoncxx::ExtendedJsonMode::k_canonical);
                   },
               },
               value._value);

    return out;
}

UniqueExpression Expression::parseExpression(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an expression");
    }

    auto nodeIt = node.begin();
    if (nodeIt == node.end()) {
        throw InvalidValueGeneratorSyntax(
            "Expected mapping to have a single '^'-prefixed key, but was empty");
    }

    auto key = nodeIt->first;
    auto value = nodeIt->second;

    ++nodeIt;
    if (nodeIt != node.end()) {
        throw InvalidValueGeneratorSyntax(
            "Expected mapping to have a single '^'-prefixed key, but had multiple keys");
    }

    auto parserIt = parserMap.find(key.as<std::string>());
    if (parserIt == parserMap.end()) {
        std::stringstream error;
        error << "Unknown expression type '" << key.as<std::string>() << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }

    return parserIt->second(value, rng);
}

UniqueExpression Expression::parseObject(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
    }

    auto nodeIt = node.begin();
    if (nodeIt != node.end()) {
        auto key = nodeIt->first;

        ++nodeIt;
        if (nodeIt == node.end() && key.as<std::string>()[0] == '^') {
            return Expression::parseExpression(node, rng);
        }
    }

    return DocumentExpression::parse(node, rng);
}

UniqueExpression Expression::parseOperand(YAML::Node node, DefaultRandom& rng) {
    switch (node.Type()) {
        case YAML::NodeType::Map:
            return Expression::parseObject(node, rng);
        case YAML::NodeType::Sequence:
            return ArrayExpression::parse(node, rng);
        case YAML::NodeType::Scalar:
        case YAML::NodeType::Null:
            return ConstantExpression::parse(node, rng);
        case YAML::NodeType::Undefined:
            throw InvalidValueGeneratorSyntax(
                "C++ programmer error: Failed to check for node's existence before attempting to"
                " parse it");
    }

    std::abort();
}

ConstantExpression::ConstantExpression(Value value, ValueType type)
    : _value(Value{std::move(value)}), _type{type} {}

UniqueExpression ConstantExpression::parse(YAML::Node node, DefaultRandom& rng) {
    switch (node.Type()) {
        case YAML::NodeType::Map: {
            auto elements = std::vector<DocumentExpression::ElementType>{};
            for (auto&& entry : node) {
                elements.emplace_back(entry.first.as<std::string>(),
                                      ConstantExpression::parse(entry.second, rng));
            }

            return std::make_unique<DocumentExpression>(std::move(elements));
        }
        case YAML::NodeType::Sequence: {
            auto elements = std::vector<ArrayExpression::ElementType>{};
            for (auto&& entry : node) {
                elements.emplace_back(ConstantExpression::parse(entry, rng));
            }

            return std::make_unique<ArrayExpression>(std::move(elements));
        }
        case YAML::NodeType::Null:
            return std::make_unique<ConstantExpression>(Value{bsoncxx::types::b_null{}},
                                                        ValueType::Null);
        case YAML::NodeType::Scalar:
        case YAML::NodeType::Undefined:
            // YAML::NodeType::Scalar and YAML::NodeType::Undefined are handled below.
            break;
    }

    if (!node.IsScalar()) {
        throw InvalidValueGeneratorSyntax("Expected scalar value to parse");
    }

    // `YAML::Node::Tag() == "!"` means that the scalar value is quoted. In that case, we want to
    // avoid converting any numeric string values to numbers. See
    // https://github.com/jbeder/yaml-cpp/issues/261 for more details.
    if (node.Tag() != "!") {
        try {
            return std::make_unique<ConstantExpression>(Value{node.as<int32_t>()},
                                                        ValueType::Integer);
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return std::make_unique<ConstantExpression>(Value{node.as<int64_t>()},
                                                        ValueType::Integer);
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return std::make_unique<ConstantExpression>(Value{node.as<double>()},
                                                        ValueType::Integer);
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return std::make_unique<ConstantExpression>(Value{node.as<bool>()}, ValueType::Boolean);
        } catch (const YAML::BadConversion& e) {
        }
    }

    return std::make_unique<ConstantExpression>(Value{node.as<std::string>()}, ValueType::String);
}

Value ConstantExpression::evaluate(genny::DefaultRandom& rng) const {
    return _value;
}

DocumentExpression::DocumentExpression(std::vector<ElementType> elements)
    : _elements(std::move(elements)) {}

UniqueExpression DocumentExpression::parse(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
    }

    auto elements = std::vector<ElementType>{};
    for (auto&& entry : node) {
        if (entry.first.as<std::string>()[0] == '^') {
            throw InvalidValueGeneratorSyntax(
                "'^'-prefix keys are reserved for expressions, but attempted to parse as an"
                " object");
        }

        elements.emplace_back(entry.first.as<std::string>(),
                              Expression::parseOperand(entry.second, rng));
    }

    return std::make_unique<DocumentExpression>(std::move(elements));
}

Value DocumentExpression::evaluate(genny::DefaultRandom& rng) const {
    auto doc = bsoncxx::builder::basic::document{};

    for (auto&& elem : _elements) {
        elem.second->evaluate(rng).appendToBuilder(doc, elem.first);
    }

    return Value{doc.extract()};
}

ArrayExpression::ArrayExpression(std::vector<ElementType> elements)
    : _elements(std::move(elements)) {}

UniqueExpression ArrayExpression::parse(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsSequence()) {
        throw InvalidValueGeneratorSyntax("Expected sequence type to parse into an array");
    }

    auto elements = std::vector<ElementType>{};
    for (auto&& entry : node) {
        elements.emplace_back(Expression::parseOperand(entry, rng));
    }

    return std::make_unique<ArrayExpression>(std::move(elements));
}

Value ArrayExpression::evaluate(genny::DefaultRandom& rng) const {
    auto arr = bsoncxx::builder::basic::array{};

    for (auto&& elem : _elements) {
        elem->evaluate(rng).appendToBuilder(arr);
    }

    return Value{arr.extract()};
}

UniqueExpression RandomIntExpression::parse(YAML::Node node, DefaultRandom& rng) {
    auto distribution = node["distribution"].as<std::string>("uniform");

    if (distribution == "uniform") {
        UniqueExpression min;
        UniqueExpression max;

        if (auto entry = node["min"]) {
            min = Expression::parseOperand(entry, rng);
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'min' parameter for uniform distribution");
        }

        if (auto entry = node["max"]) {
            max = Expression::parseOperand(entry, rng);
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'max' parameter for uniform distribution");
        }

        UniqueTypedExpression<IntegerValueType> minT =
            std::make_unique<TypedExpression<IntegerValueType>>(std::move(min), rng);
        UniqueTypedExpression<IntegerValueType> maxT =
            std::make_unique<TypedExpression<IntegerValueType>>(std::move(max), rng);

        return std::make_unique<UniformIntExpression>(std::move(minT), std::move(maxT));
    } else if (distribution == "binomial") {
        UniqueExpression t;
        double p;

        if (auto entry = node["t"]) {
            t = Expression::parseOperand(entry, rng);
        } else {
            throw InvalidValueGeneratorSyntax("Expected 't' parameter for binomial distribution");
        }

        if (auto entry = node["p"]) {
            p = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'p' parameter for binomial distribution");
        }

        UniqueTypedExpression<IntegerValueType> tTyped =
            std::make_unique<TypedExpression<IntegerValueType>>(std::move(t), rng);

        return std::make_unique<BinomialIntExpression>(std::move(tTyped), p);
    } else if (distribution == "negative_binomial") {
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
            std::make_unique<TypedExpression<IntegerValueType>>(std::move(k), rng);

        return std::make_unique<NegativeBinomialIntExpression>(std::move(kTyped), p);
    } else if (distribution == "geometric") {
        double p;

        if (auto entry = node["p"]) {
            p = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'p' parameter for geometric distribution");
        }

        return std::make_unique<GeometricIntExpression>(p);
    } else if (distribution == "poisson") {
        double mean;

        if (auto entry = node["mean"]) {
            mean = entry.as<double>();
        } else {
            throw InvalidValueGeneratorSyntax("Expected 'mean' parameter for poisson distribution");
        }

        return std::make_unique<PoissonIntExpression>(mean);
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }
}

UniformIntExpression::UniformIntExpression(UniqueTypedExpression<IntegerValueType> min,
                                           UniqueTypedExpression<IntegerValueType> max)
    : _min(std::move(min)), _max(std::move(max)) {}

Value UniformIntExpression::evaluate(genny::DefaultRandom& rng) const {
    auto min = getInt64Parameter(_min->evaluate(), "min");
    auto max = getInt64Parameter(_max->evaluate(), "max");

    auto distribution = std::uniform_int_distribution<int64_t>{min, max};
    return Value{distribution(rng)};
}

BinomialIntExpression::BinomialIntExpression(UniqueTypedExpression<IntegerValueType> t, double p)
    : _t(std::move(t)), _p(p) {}

Value BinomialIntExpression::evaluate(genny::DefaultRandom& rng) const {
    auto t = getInt64Parameter(_t->evaluate(), "t");

    auto distribution = std::binomial_distribution<int64_t>{t, _p};
    return Value{distribution(rng)};
}

NegativeBinomialIntExpression::NegativeBinomialIntExpression(
    UniqueTypedExpression<IntegerValueType> k, double p)
    : _k(std::move(k)), _p(p) {}

Value NegativeBinomialIntExpression::evaluate(genny::DefaultRandom& rng) const {
    auto k = getInt64Parameter(_k->evaluate(), "k");

    auto distribution = std::negative_binomial_distribution<int64_t>{k, _p};
    return Value{distribution(rng)};
}

GeometricIntExpression::GeometricIntExpression(double p) : _p(p) {}

Value GeometricIntExpression::evaluate(genny::DefaultRandom& rng) const {
    auto distribution = std::geometric_distribution<int64_t>{_p};
    return Value{distribution(rng)};
}

PoissonIntExpression::PoissonIntExpression(double mean) : _mean(mean) {}

Value PoissonIntExpression::evaluate(genny::DefaultRandom& rng) const {
    auto distribution = std::poisson_distribution<int64_t>{_mean};
    return Value{distribution(rng)};
}

RandomStringExpression::RandomStringExpression(UniqueTypedExpression<IntegerValueType> length,
                                               std::optional<std::string> alphabet)
    : _length(std::move(length)), _alphabet(std::move(alphabet)) {}

UniqueExpression RandomStringExpression::parse(YAML::Node node, DefaultRandom& rng) {
    UniqueExpression length;
    std::optional<std::string> alphabet;

    if (auto entry = node["length"]) {
        length = Expression::parseOperand(entry, rng);
    } else {
        throw InvalidValueGeneratorSyntax(
            "Expected 'length' parameter for random string generator");
    }

    if (auto entry = node["alphabet"]) {
        alphabet = entry.as<std::string>();

        if (alphabet->empty()) {
            throw InvalidValueGeneratorSyntax(
                "Expected non-empty 'alphabet' parameter for random string generator");
        }
    }

    UniqueTypedExpression<IntegerValueType> lengthT =
        std::make_unique<TypedExpression<IntegerValueType>>(std::move(length), rng);

    // TODO: does this want to be `optional`?
    return std::make_unique<RandomStringExpression>(std::move(lengthT), std::move(alphabet));
}

Value RandomStringExpression::evaluate(genny::DefaultRandom& rng) const {
    auto alphabet = _alphabet ? std::string_view{_alphabet.value()} : kDefaultAlphabet;
    auto alphabetLength = alphabet.size();

    auto distribution = std::uniform_int_distribution<size_t>{0, alphabetLength - 1};

    auto length = getInt64Parameter(_length->evaluate(), "length");
    std::string str(length, '\0');

    for (int i = 0; i < length; ++i) {
        str[i] = alphabet[distribution(rng)];
    }

    return Value{str};
}

FastRandomStringExpression::FastRandomStringExpression(
    UniqueTypedExpression<IntegerValueType> length)
    : _length(std::move(length)) {}

UniqueExpression FastRandomStringExpression::parse(YAML::Node node, DefaultRandom& rng) {
    UniqueExpression length;

    if (auto entry = node["length"]) {
        length = Expression::parseOperand(entry, rng);
    } else {
        throw InvalidValueGeneratorSyntax("Expected 'length' parameter for fast random string");
    }

    UniqueTypedExpression<IntegerValueType> lengthT =
        std::make_unique<TypedExpression<IntegerValueType>>(std::move(length), rng);
    return std::make_unique<FastRandomStringExpression>(std::move(lengthT));
}

Value FastRandomStringExpression::evaluate(genny::DefaultRandom& rng) const {
    auto length = getInt64Parameter(_length->evaluate(), "length");
    std::string str(length, '\0');

    auto randomValue = rng();
    int bits = 64;

    for (int i = 0; i < length; ++i) {
        if (bits < 6) {
            randomValue = rng();
            bits = 64;
        }

        str[i] = kAlphabet[(randomValue & 0x2f) % kAlphabetLength];
        randomValue >>= 6;
        bits -= 6;
    }

    return Value{str};
}

}  // namespace genny::value_generators::v1
