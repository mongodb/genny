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

#include <testlib/yamlToBson.hpp>

#include <optional>
#include <variant>

#include <boost/throw_exception.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

namespace genny::testing {

namespace {
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

std::string to_string(const YAML::Node& node) {
    YAML::Emitter e;
    e << node;
    return std::string{e.c_str()};
}


Value parseScalar(const YAML::Node& node) {
    if (!node.IsScalar() && !node.IsNull()) {
        std::stringstream msg;
        msg << "Expected scalar or null got " << node.Type();
        BOOST_THROW_EXCEPTION(InvalidYAMLToBsonException(msg.str()));
    }

    if (node.Type() == YAML::NodeType::Null) {
        return Value{bsoncxx::types::b_null{}};
    }

    // `YAML::Node::Tag() == "!"` means that the scalar value is quoted. In that case, we want to
    // avoid converting any numeric string values to numbers. See
    // https://github.com/jbeder/yaml-cpp/issues/261 for more details.
    if (node.Tag() != "!") {
        try {
            return Value{node.as<int32_t>()};
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return Value{node.as<int64_t>()};
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return Value{node.as<double>()};
        } catch (const YAML::BadConversion& e) {
        }

        try {
            return Value{node.as<bool>()};
        } catch (const YAML::BadConversion& e) {
        }
    }

    return Value{node.as<std::string>()};
}

void appendToBuilder(const YAML::Node& node,
                     const std::string& key,
                     bsoncxx::builder::basic::document& doc) {
    switch (node.Type()) {
        case YAML::NodeType::Map:
            doc.append(bsoncxx::builder::basic::kvp(key, toDocumentBson(node)));
            return;
        case YAML::NodeType::Sequence:
            doc.append(bsoncxx::builder::basic::kvp(key, toArrayBson(node)));
            return;
        case YAML::NodeType::Undefined:
        case YAML::NodeType::Scalar:
        case YAML::NodeType::Null: {
            Value val = parseScalar(node);
            val.appendToBuilder(doc, key);
            return;
        }
    }

    std::abort();
}

void appendToBuilder(const YAML::Node& node, bsoncxx::builder::basic::array& arr) {
    switch (node.Type()) {
        case YAML::NodeType::Map:
            arr.append(toDocumentBson(node));
            return;
        case YAML::NodeType::Sequence:
            arr.append(toArrayBson(node));
            return;
        case YAML::NodeType::Undefined:
        case YAML::NodeType::Scalar:
        case YAML::NodeType::Null: {
            Value val = parseScalar(node);
            val.appendToBuilder(arr);
            return;
        }
    }

    std::abort();
}

}  // namespace
}  // namespace genny::testing


bsoncxx::document::view_or_value genny::testing::toDocumentBson(const YAML::Node& node) {
    if (!node.IsMap()) {
        std::stringstream msg;
        msg << "Wanted map got " << node.Type() << ": " << to_string(node);
        BOOST_THROW_EXCEPTION(InvalidYAMLToBsonException(msg.str()));
    }

    bsoncxx::builder::basic::document doc{};
    for (auto&& kvp : node) {
        appendToBuilder(kvp.second, kvp.first.as<std::string>(), doc);
    }
    return doc.extract();
}

bsoncxx::array::view_or_value genny::testing::toArrayBson(const YAML::Node& node) {
    if (!node.IsSequence()) {
        std::stringstream msg;
        msg << "Wanted sequence got " << node.Type() << ": " << to_string(node);
        BOOST_THROW_EXCEPTION(InvalidYAMLToBsonException(msg.str()));
    }

    bsoncxx::builder::basic::array arr{};
    for (auto&& elt : node) {
        appendToBuilder(elt, arr);
    }
    return arr.extract();
}
