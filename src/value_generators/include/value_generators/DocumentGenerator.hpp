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

#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/view_or_value.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include <yaml-cpp/yaml.h>

#include <value_generators/DefaultRandom.hpp>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidValueGeneratorSyntax : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

namespace v1 {

/**
 * A variant type for representing different kinds of values that can be generated by an Expression
 * instance.
 *
 * The bsoncxx::types::b_document and bsoncxx::types::b_array cases that are part of
 * bsoncxx::types::value do not allow for BSON documents or arrays which own their underlying buffer
 * and instead can only be constructed from their respective view types. This is incompatible with
 * Expression::evaluate() because the generated (owning) BSON document or array must outlive its
 * view. Additionally, the Value class uses the bsoncxx::document::view_or_value and
 * bsoncxx::array::view_or_value types to represent BSON documents and arrays that may optionally
 * own their underlying buffer. Allowing for view types leaves the possibility open for never
 * copying a fixed document or array value once it is parsed from the configuration file.
 *
 * Note that the Value class encapsulates std::variant rather than aliases it in order to define a
 * custom std::ostream::operator<< for it.
 */
class Value {
public:
    //
    // Constructors
    //

    explicit Value(bool value);
    explicit Value(int32_t value);
    explicit Value(int64_t value);
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(bsoncxx::types::b_null value);
    explicit Value(bsoncxx::document::view_or_value value);
    explicit Value(bsoncxx::array::view_or_value value);

    //
    // Getters
    //

    bool getBool() const;
    int32_t getInt32() const;
    int64_t getInt64() const;
    double getDouble() const;
    std::string getString() const;
    bsoncxx::types::b_null getNull() const;
    bsoncxx::document::view_or_value getDocument() const;
    bsoncxx::array::view_or_value getArray() const;

    /**
     * Return `_value` as an int64_t if it is of type int32_t or int64_t, or std::nullopt if it is
     * some other type.
     */
    std::optional<int64_t> tryAsInt64() const;

    /**
     * Append the underlying `_value` to the document `doc` using the field name `key`.
     *
     * @warning
     *  After calling appendToBuilder() it is illegal to call any methods on this instance, unless
     *  it is subsequently moved into.
     */
    void appendToBuilder(bsoncxx::builder::basic::document& doc, std::string key);

    /**
     * Append the underlying `_value` to the array `arr`.
     *
     * @warning
     *  After calling appendToBuilder() it is illegal to call any methods on this instance, unless
     *  it is subsequently moved into.
     */
    void appendToBuilder(bsoncxx::builder::basic::array& arr);

private:
    using VariantType = std::variant<bool,
                                     int32_t,
                                     int64_t,
                                     double,
                                     std::string,
                                     bsoncxx::types::b_null,
                                     bsoncxx::document::view_or_value,
                                     bsoncxx::array::view_or_value>;

    friend std::ostream& operator<<(std::ostream& out, const Value& value);

    VariantType _value;
};


class Expression;
using UniqueExpression = std::unique_ptr<Expression>;


enum class ValueType {
    Integer,
    Double,
    String,
    Array,
    Boolean,
    Document,
    Null,
};

inline std::ostream& operator<<(std::ostream& out, ValueType t) {
    switch (t) {
        case ValueType::Integer:
            out << "Integer";
            break;
        case ValueType::Double:
            out << "Double";
            break;
        case ValueType::String:
            out << "String";
            break;
        case ValueType::Array:
            out << "Array";
            break;
        case ValueType::Boolean:
            out << "Boolean";
            break;
        case ValueType::Document:
            out << "Document";
            break;
        case ValueType::Null:
            out << "Null";
            break;
    }
    return out;
}

struct IntegerValueType {
    using OutputType = int64_t;
    constexpr static ValueType valueType() {
        return ValueType::Integer;
    }
    static OutputType convert(const Value& value) {
        auto out = value.tryAsInt64();
        if (!out) {
            std::stringstream error;
            error << "Expected integer, but got " << value;
            throw InvalidValueGeneratorSyntax(error.str());
        }
        return *out;
    }
};

struct DoubleValueType {
    using OutputType = double;
    constexpr static ValueType valueType() {
        return ValueType::Double;
    }
    static OutputType convert(const Value& value) {
        auto maybeInt = value.tryAsInt64();
        if (maybeInt) {
            return double(*maybeInt);
        }
        return value.getDouble();
    }
};

struct DocumentValueType {
    using OutputType = bsoncxx::document::view_or_value;
    constexpr static ValueType valueType() {
        return ValueType::Document;
    }
    static OutputType convert(const Value& value) {
        return value.getDocument();
    }
};


/**
 * Base class for generating values.
 *
 * Its API is modeled off the Expression class for representing aggregation expressions in the
 * MongoDB Server.
 */
class Expression {
public:
    using Parser = std::function<UniqueExpression(YAML::Node, DefaultRandom& rng)>;

    /**
     * Parse `node` into one of the Expression types registered in the `parserMap`.
     *
     * @throws InvalidConfigurationException if `node` isn't a mapping type with a '^'-prefixed key.
     */
    static UniqueExpression parseExpression(YAML::Node node, DefaultRandom& rng);

    /**
     * Parse `node` into either:
     *   - one of the Expression types registered in the `parserMap`, or
     *   - a DocumentExpression instance containing further nested Expression types.
     *
     * @throws InvalidConfigurationException if `node` isn't a mapping type.
     */
    static UniqueExpression parseObject(YAML::Node node, DefaultRandom& rng);

    /**
     * Parse `node` into one of the following:
     *   - one of the Expression types registered in the `parserMap`,
     *   - a DocumentExpression instance containing further nested Expression types,
     *   - an ArrayExpression instance containing a list Expression types,
     *   - a ConstantExpression instance representing a scalar value.
     *
     * @throws InvalidConfigurationException if `node` is malformed.
     */
    static UniqueExpression parseOperand(YAML::Node node, DefaultRandom& rng);

    virtual ~Expression() = default;

    virtual ValueType valueType() const = 0;

    /**
     * Generate a possibly randomized value.
     *
     * @throws InvalidConfigurationException if the arguments to this Expression evaluate an
     * unexpected type.
     */
    virtual Value evaluate(genny::DefaultRandom& rng) const = 0;
};

template <class T>
class TypedExpression {
    using OutputType = typename T::OutputType;

public:
    TypedExpression(UniqueExpression expression) : _expression{std::move(expression)} {
        auto actual = _expression->valueType();
        if (actual != t) {
            std::stringstream msg;
            msg << "Expected " << t << " but got " << actual;
            throw InvalidValueGeneratorSyntax(msg.str());
        }
    }
    OutputType evaluate(DefaultRandom& rng) {
        return convert(_expression->evaluate(rng));
    }

private:
    UniqueExpression _expression;
    static constexpr ValueType t = T::valueType();

    static OutputType convert(const Value& value) {
        return T::convert(value);
    }
};

template <typename T>
using UniqueTypedExpression = std::unique_ptr<TypedExpression<T>>;

/**
 * Class for representing a fixed scalar value.
 */
class ConstantExpression : public Expression {
public:
    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);

    explicit ConstantExpression(Value value, ValueType type);
    Value evaluate(genny::DefaultRandom& rng) const override;
    ValueType valueType() const override {
        return _type;
    }

private:
    const ValueType _type;
    const Value _value;
};


/**
 * Class for generating a bsoncxx::document::value with its elements generated from other Expression
 * types.
 */
class DocumentExpression : public Expression {
public:
    using ElementType = std::pair<std::string, UniqueExpression>;

    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);

    explicit DocumentExpression(std::vector<ElementType> elements);
    Value evaluate(genny::DefaultRandom& rng) const override;

    ValueType valueType() const override {
        return ValueType::Document;
    }

private:
    const std::vector<ElementType> _elements;
};


/**
 * Class for generating a bsoncxx::array::value with its elements generated from other Expression
 * types.
 */
class ArrayExpression : public Expression {
public:
    using ElementType = UniqueExpression;

    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);

    explicit ArrayExpression(std::vector<ElementType> elements);
    Value evaluate(genny::DefaultRandom& rng) const override;
    ValueType valueType() const override {
        return ValueType::Array;
    }


private:
    const std::vector<ElementType> _elements;
};


/**
 * Base class for generating random int64_t values.
 */
class RandomIntExpression : public Expression {
public:
    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);
    virtual ValueType valueType() const override {
        return ValueType::Integer;
    }
};


/**
 * Class for generating random int64_t values according to a uniform distribution.
 *
 * See std::uniform_int_distribution for more details.
 */
class UniformIntExpression : public RandomIntExpression {
public:
    UniformIntExpression(UniqueTypedExpression<IntegerValueType> min,
                         UniqueTypedExpression<IntegerValueType> max);
    Value evaluate(genny::DefaultRandom& rng) const override;

private:
    const UniqueTypedExpression<IntegerValueType> _min;
    const UniqueTypedExpression<IntegerValueType> _max;
};


/**
 * Class for generating random int64_t values according to a binomial distribution.
 *
 * See std::binomial_distribution for more details.
 */
class BinomialIntExpression : public RandomIntExpression {
public:
    BinomialIntExpression(UniqueTypedExpression<IntegerValueType> t, UniqueTypedExpression<DoubleValueType> p);
    Value evaluate(genny::DefaultRandom& rng) const override;

private:
    const UniqueTypedExpression<IntegerValueType> _t;
    const UniqueTypedExpression<DoubleValueType> _p;
};


/**
 * Class for generating random int64_t values according to a negative binomial distribution.
 *
 * See std::negative_binomial_distribution for more details.
 */
class NegativeBinomialIntExpression : public RandomIntExpression {
public:
    NegativeBinomialIntExpression(UniqueTypedExpression<IntegerValueType> k, double p);
    Value evaluate(genny::DefaultRandom& rng) const override;

private:
    const UniqueTypedExpression<IntegerValueType> _k;
    const double _p;
};


/**
 * Class for generating random int64_t values according to a geometric distribution.
 *
 * See std::geometric_distribution for more details.
 */
class GeometricIntExpression : public RandomIntExpression {
public:
    explicit GeometricIntExpression(UniqueTypedExpression<DoubleValueType> p);
    Value evaluate(genny::DefaultRandom& rng) const override;

private:
    const UniqueTypedExpression<DoubleValueType> _p;
};


/**
 * Class for generating random int64_t values according to a poisson distribution.
 *
 * See std::poisson_distribution for more details.
 */
class PoissonIntExpression : public RandomIntExpression {
public:
    explicit PoissonIntExpression(UniqueTypedExpression<DoubleValueType> mean);
    Value evaluate(genny::DefaultRandom& rng) const override;

private:
    const UniqueTypedExpression<DoubleValueType> _mean;
};

/**
 * Class for generating random std::string values with a specified length and using an optionally
 * specified alphabet.
 */
class RandomStringExpression : public Expression {
public:
    static constexpr auto kDefaultAlphabet = std::string_view{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/"};

    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);

    RandomStringExpression(UniqueTypedExpression<IntegerValueType> length,
                           std::optional<std::string> alphabet);
    Value evaluate(genny::DefaultRandom& rng) const override;
    ValueType valueType() const override {
        return ValueType::String;
    }


private:
    const UniqueTypedExpression<IntegerValueType> _length;
    const std::optional<std::string> _alphabet;
};

/**
 * Class for generating random std::string values with a specified length and a fixed alphabet.
 */
class FastRandomStringExpression : public Expression {
public:
    static constexpr auto kAlphabet = RandomStringExpression::kDefaultAlphabet;
    static constexpr auto kAlphabetLength = kAlphabet.size();

    static UniqueExpression parse(YAML::Node node, DefaultRandom& rng);

    explicit FastRandomStringExpression(UniqueTypedExpression<IntegerValueType> length);
    Value evaluate(genny::DefaultRandom& rng) const override;
    ValueType valueType() const override {
        return ValueType::String;
    }

private:
    const UniqueTypedExpression<IntegerValueType> _length;
};

}  // namespace v1

class DocumentGenerator {
public:
    static DocumentGenerator create(YAML::Node node, DefaultRandom& rng) {
        return DocumentGenerator{std::make_unique<v1::TypedExpression<v1::DocumentValueType>>(
                                     v1::Expression::parseOperand(node, rng)),
                                 rng};
    }

    auto operator()() {
        return _ptr->evaluate(_rng);
    }

private:
    using UPtr = v1::UniqueTypedExpression<v1::DocumentValueType>;

    UPtr _ptr;
    DefaultRandom& _rng;

    DocumentGenerator(UPtr ptr, DefaultRandom& rng) : _ptr{std::move(ptr)}, _rng{rng} {}
};

}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
