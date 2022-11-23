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

#ifndef HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED
#define HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED

#include <string_view>

#include <boost/throw_exception.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny {

template <>
struct NodeConvert<mongocxx::options::aggregate> {
    using type = mongocxx::options::aggregate;
    static type convert(const Node& node) {
        type rhs{};

        if (node["AllowDiskUse"]) {
            auto allowDiskUse = node["AllowDiskUse"].to<bool>();
            rhs.allow_disk_use(allowDiskUse);
        }
        if (node["BatchSize"]) {
            auto batchSize = node["BatchSize"].to<int>();
            rhs.batch_size(batchSize);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds(maxTime));
        }
        if (node["ReadPreference"]) {
            auto readPreference = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPreference);
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassValidation = node["BypassDocumentValidation"].to<bool>();
            rhs.bypass_document_validation(bypassValidation);
        }
        if (node["Hint"]) {
            auto h = node["Hint"].to<std::string>();
            auto hint = mongocxx::hint(std::move(h));
            rhs.hint(hint);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::bulk_write> {
    using type = mongocxx::options::bulk_write;

    static type convert(const Node& node) {
        type rhs{};

        if (node["BypassDocumentValidation"]) {
            auto bypassDocValidation = node["BypassDocumentValidation"].to<bool>();
            rhs.bypass_document_validation(bypassDocValidation);
        }
        if (node["Ordered"]) {
            auto isOrdered = node["Ordered"].to<bool>();
            rhs.ordered(isOrdered);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::count> {
    using type = mongocxx::options::count;

    static type convert(const Node& node) {
        type rhs{};

        if (node["Hint"]) {
            auto h = node["Hint"].to<std::string>();
            auto hint = mongocxx::hint(std::move(h));
            rhs.hint(hint);
        }
        if (node["Limit"]) {
            auto limit = node["Limit"].to<int>();
            rhs.limit(limit);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return rhs;
    }
};

// TODO EVG-18364: This code is duplicated from yamlToBson.hpp to elide a linking issue. Deduplicate
//  this code.
namespace {
class InvalidYAMLToBsonException : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

/**
 * @param node map node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a map
 */
bsoncxx::document::value toDocumentBson(const YAML::Node& node);

/**
 * @param node map node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a map
 */
bsoncxx::document::value toDocumentBson(const std::string& yaml);

/**
 * @param node map node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a map
 */
bsoncxx::document::value toDocumentBson(const genny::Node& node);


/**
 * @param node list node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a list (sequence)
 */
bsoncxx::array::value toArrayBson(const YAML::Node& node);

/**
 * @param node list node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a list (sequence)
 */
bsoncxx::array::value toArrayBson(const std::string& node);

/**
 * @param node yaml list node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a list (sequence)
 */
bsoncxx::array::value toArrayBson(const genny::Node& node);

// Helper type cribbed from value_generators
// The value_generators version will likely disappear soon.
class Value {
public:
    explicit Value(bool value) : _value{value} {}
    explicit Value(int32_t value) : _value{value} {}
    explicit Value(int64_t value) : _value{value} {}
    explicit Value(double value) : _value{value} {}
    explicit Value(std::string value) : _value{value} {}
    explicit Value(bsoncxx::types::b_null value) : _value{value} {}
    void appendToBuilder(bsoncxx::builder::basic::document& doc, std::string key) {
        std::visit(
            [&](auto&& arg) { doc.append(bsoncxx::builder::basic::kvp(std::move(key), arg)); },
            std::move(_value));
    }
    void appendToBuilder(bsoncxx::builder::basic::array& arr) {
        std::visit([&](auto&& arg) { arr.append(arg); }, std::move(_value));
    }

    static Value parseScalar(const YAML::Node& node) {
        if (!node.IsScalar() && !node.IsNull()) {
            std::stringstream msg;
            msg << "Expected scalar or null got " << node.Type();
            BOOST_THROW_EXCEPTION(InvalidYAMLToBsonException(msg.str()));
        }

        if (node.Type() == YAML::NodeType::Null) {
            return Value{bsoncxx::types::b_null{}};
        }

        // `YAML::Node::Tag() == "!"` means that the scalar value is quoted. In that case, we want
        // to avoid converting any numeric string values to numbers. See
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

private:
    using VariantType =
        std::variant<bool, int32_t, int64_t, double, std::string, bsoncxx::types::b_null>;
    VariantType _value;
};


std::string to_string(const YAML::Node& node) {
    YAML::Emitter e;
    e << node;
    return std::string{e.c_str()};
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
            Value val = Value::parseScalar(node);
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
            Value val = Value::parseScalar(node);
            val.appendToBuilder(arr);
            return;
        }
    }

    std::abort();
}

bsoncxx::document::value toDocumentBson(const YAML::Node& node) {
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

bsoncxx::array::value toArrayBson(const YAML::Node& node) {
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

bsoncxx::document::value toDocumentBson(const genny::Node& node) {
    std::stringstream str;
    str << node;
    const YAML::Node asYaml = YAML::Load(str.str());
    return genny::toDocumentBson(asYaml);
}

bsoncxx::document::value toDocumentBson(const std::string& yaml) {
    const YAML::Node asYaml = YAML::Load(yaml);
    return genny::toDocumentBson(asYaml);
}

bsoncxx::array::value toArrayBson(const std::string& yaml) {
    const YAML::Node asYaml = YAML::Load(yaml);
    return genny::toArrayBson(asYaml);
}

}  // namespace

template <>
struct NodeConvert<bsoncxx::document::value> {
    using type = bsoncxx::document::value;

    static type convert(const Node& node) {
        return toDocumentBson(node);
    }
};

template <>
struct NodeConvert<mongocxx::options::find> {
    using type = mongocxx::options::find;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& sort = node["Sort"]) {
            rhs.sort(sort.to<bsoncxx::document::value>());
        }
        if (const auto& collation = node["Collation"]) {
            rhs.collation(collation.to<bsoncxx::document::value>());
        }
        if (const auto& hint = node["Hint"]) {
            rhs.hint(mongocxx::hint(std::move(hint.to<std::string>())));
        }
        if (const auto& comment = node["Comment"]) {
            rhs.comment(comment.to<std::string>());
        }
        if (const auto& limit = node["Limit"]) {
            rhs.limit(limit.to<long>());
        }
        if (const auto& skip = node["Skip"]) {
            rhs.skip(skip.to<long>());
        }
        if (const auto& batchSize = node["BatchSize"]) {
            rhs.batch_size(batchSize.to<long>());
        }
        if (const auto& maxTime = node["MaxTime"]) {
            rhs.max_time(std::chrono::milliseconds{node["MaxTime"].to<int>()});
        }
        if (const auto& readPref = node["ReadPreference"]) {
            rhs.read_preference(readPref.to<mongocxx::read_preference>());
        }

        auto getBoolValue = [&](const std::string& paramName) {
            const auto& val = node[paramName];
            return val ? val.to<bool>() : false;
        };

        // Figure out the cursor type.
        const bool tailable = getBoolValue("Tailable");
        const bool awaitData =  getBoolValue("AwaitData");
        if(tailable && awaitData){
            rhs.cursor_type(mongocxx::cursor::type::k_tailable_await);
        } else if (tailable) {
            rhs.cursor_type(mongocxx::cursor::type::k_tailable);
        } else if (awaitData) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Cannot set 'awaitData' to true without also setting 'tailable' to true"));
        } else {
            rhs.cursor_type(mongocxx::cursor::type::k_non_tailable);
        }
        return rhs;
    }
};


template <>
struct NodeConvert<mongocxx::options::estimated_document_count> {
    using type = mongocxx::options::estimated_document_count;

    static type convert(const Node& node) {
        type rhs{};

        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return rhs;
    }
};


template <>
struct NodeConvert<mongocxx::options::insert> {
    using type = mongocxx::options::insert;

    static type convert(const Node& node) {
        type rhs{};

        if (node["Ordered"]) {
            rhs.ordered(node["Ordered"].to<bool>());
        }
        if (node["BypassDocumentValidation"]) {
            rhs.bypass_document_validation(node["BypassDocumentValidation"].to<bool>());
        }
        if (node["WriteConcern"]) {
            rhs.write_concern(node["WriteConcern"].to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::transaction> {
    using type = mongocxx::options::transaction;

    static type convert(const Node& node) {
        type rhs{};

        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        if (node["ReadConcern"]) {
            auto rc = node["ReadConcern"].to<mongocxx::read_concern>();
            rhs.read_concern(rc);
        }
        if (node["MaxCommitTime"]) {
            auto maxCommitTime = node["MaxCommitTime"].to<genny::TimeSpec>();
            rhs.max_commit_time_ms(std::chrono::milliseconds(maxCommitTime));
        }
        if (node["ReadPreference"]) {
            auto rp = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(rp);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::update> {
    using type = mongocxx::options::update;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& bypass = node["Bypass"]; bypass) {
            rhs.bypass_document_validation(bypass.to<bool>());
        }
        if (const auto& hint = node["Hint"]; hint) {
            rhs.hint(mongocxx::hint(std::move(hint.to<std::string>())));
        }
        if (const auto& upsert = node["Upsert"]; upsert) {
            rhs.upsert(upsert.to<bool>());
        }
        if (const auto& wc = node["WriteConcern"]; wc) {
            rhs.write_concern(wc.to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::delete_options> {
    using type = mongocxx::options::delete_options;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& hint = node["Hint"]; hint) {
            rhs.hint(mongocxx::hint(std::move(hint.to<std::string>())));
        }
        if (const auto& wc = node["WriteConcern"]; wc) {
            rhs.write_concern(wc.to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

}  // namespace genny
#endif  // HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED
