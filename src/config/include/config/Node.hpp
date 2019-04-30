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

#ifndef HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
#define HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/throw_exception.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

namespace v1 {
template <typename T>
std::string toString(const T& t) {
    std::ostringstream out;
    out << t;
    return out.str();
}
}  // namespace v1

// We don't actually care about YAML::convert<T>
// because we never go from T to YAML::Node just the
// other way around.
//
// This template *may* be specialized by the user if
// they wish to do special post-processing (e.g. if
// the O type is a ptr-type and want to do dynamic_cast)
template <typename O>
struct NodeConvert {};


/**
 * Throw this to indicate a bad path
 */
class InvalidPathException : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class Node {
public:
    enum class NodeType {
        Undefined,
        Null,
        Scalar,
        Sequence,
        Map,
    };

    Node(const std::string& yaml, std::string key) : Node{parse(yaml), nullptr, std::move(key)} {}

    // explicitly allow copy and move
    // this is here to protect against regressions
    // accidentally making this non-copy/move-able

    Node(const Node&) = default;
    Node(Node&&) = default;
    Node& operator=(const Node&) = default;
    Node& operator=(Node&&) = default;

    template <typename T>
    T value_or(T&& fallback) const {
        if (!(*this) || this->isNull()) {
            return fallback;
        }
        if (_yaml) {
            return to<T>();
        } else {
            return fallback;
        }
    }

    template <typename O, typename... Args>
    O to(Args&&... args) const {
        auto out = maybe<O, Args...>(std::forward<Args>(args)...);
        if (!out) {
            std::ostringstream msg;
            msg << "Tried to access node that doesn't exist at path: " << this->path();
            // TODO: custom exception type
            BOOST_THROW_EXCEPTION(std::logic_error(msg.str()));
        }
        return *out;
    }

    template <typename O = Node, typename... Args>
    std::optional<O> maybe(Args&&... args) const {
        // TODO: tests of this
        static_assert(!std::is_same_v<std::decay_t<YAML::Node>, std::decay_t<O>>, "🙈 YAML::Node");
        static_assert(
            std::is_same_v<decltype(_maybeImpl<O, Args...>(std::forward<Args>(args)...)),
                           std::optional<O>>,
            "Destination type must satisfy at least one of the following:\n"
            "\n"
            "1.  is constructible from `Node&` and the given arguments\n"
            "2.  has a `NodeConvert` struct like the following:\n"
            "\n"
            "        namespace genny {\n"
            "        template<> struct NodeConvert<Foo> {\n"
            "            using type = Foo;\n"
            "            static Foo convert(const Node& node, other, args) { ... }\n"
            "        };\n"
            "        }  // namesace genny\n"
            "\n"
            "3.  is a type built into YAML::Node e.g. int, string, vector<built-in-type> etc.");
        if (!*this) {
            return std::nullopt;
        }
        return _maybeImpl<O, Args...>(std::forward<Args>(args)...);
    }

    template <typename K>
    const Node operator[](const K& key) const {
        return this->get(key);
    }

    auto size() const {
        return _yaml.size();
    }

    explicit operator bool() const {
        return _valid && _yaml;
    }

    bool isNull() const {
        return type() == NodeType::Null;
    }
    bool isScalar() const {
        return type() == NodeType::Scalar;
    }
    bool isSequence() const {
        return type() == NodeType::Sequence;
    }
    bool isMap() const {
        return type() == NodeType::Map;
    }

    NodeType type() const {
        if (!*this) {
            return NodeType::Undefined;
        }
        auto yamlTyp = _yaml.Type();
        switch (yamlTyp) {
            case YAML::NodeType::Undefined:
                return NodeType::Undefined;
            case YAML::NodeType::Null:
                return NodeType::Null;
            case YAML::NodeType::Scalar:
                return NodeType::Scalar;
            case YAML::NodeType::Sequence:
                return NodeType::Sequence;
            case YAML::NodeType::Map:
                return NodeType::Map;
        }
    }

    std::string path() const;

    friend class IteratorValue;

    struct iterator;
    iterator begin() const;
    iterator end() const;


    /**
     * Extract a vector of items by supporting both singular and plural keys.
     *
     * Example YAML that this supports:
     *
     * ```yaml
     * # Calling getPlural<int>("Number","Numbers") returns [7]
     * Foo:
     *   Number: 7
     *
     * # Calling getPlural<int>("Number","Numbers") returns [1,2]
     * Bar:
     *   Numbers: [1,2]
     * ```
     *
     * The node cannot have both keys present. The following
     * will error:
     *
     * ```yaml
     * # Calling getPlural<int>("Bad","Bads")
     * # will throw because the node must have
     * # exactly one of the keys
     * BadExample:
     *   Bad: 7
     *   Bads: [1,2]
     * ```
     *
     * If the value at the plural key isn't a sequence we also barf:
     *
     * ```yaml
     * # Calling getPlural<int>("Bad","Bads") will fail
     * # because Bads isn't a sequence.
     * AnotherBadExample:
     *   Bads: 3
     * ```
     *
     * @tparam T the returned type. Must align with the return-type of F.
     * @tparam F type of the callback/mapping function that maps from `Node` to `T`.
     * @param singular the singular version of the key e.g. "Number"
     * @param plural the plural version of the key e.g. "Numbers"
     * @param f callback function mapping from the found node to a T instance. If not specified,
     * will use `to<T>()`
     * @return
     */
    template <typename T, typename F = std::function<T(const Node&)>>
    std::vector<T> getPlural(
        const std::string& singular, const std::string& plural, F&& f = [](const Node& n) {
            return n.to<T>();
        }) const;

private:
    Node(const YAML::Node yaml, const Node* const parent, bool valid, std::string key)
        : _yaml{yaml}, _parent{parent}, _valid{valid}, _key{std::move(key)} {}

    Node(const YAML::Node yaml, const Node* const parent, std::string key)
        : Node{yaml, parent, yaml, std::move(key)} {}


    template <typename O, typename... Args>
    static constexpr bool isNodeConstructible() {
        // TODO: test of constness
        return !std::is_trivially_constructible_v<O> &&
            (std::is_constructible_v<O, Node&, Args...> ||
             std::is_constructible_v<O, const Node&, Args...>);
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<isNodeConstructible<O, Args...>()>>
    std::optional<O> _maybeImpl(Args&&... args) const {
        return std::make_optional<O>(*this, std::forward<Args>(args)...);
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<
                  !isNodeConstructible<O, Args...>() &&
                  std::is_same_v<O, typename NodeConvert<O>::type> &&
                  std::is_same_v<O,
                                 decltype(NodeConvert<O>::convert(std::declval<Node>(),
                                                                  std::declval<Args>()...))>>,
              typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        return std::make_optional<O>(NodeConvert<O>::convert(*this, std::forward<Args>(args)...));
    }

    template <
        typename O,
        typename... Args,
        typename = std::enable_if_t<
            !isNodeConstructible<O, Args...>() &&
            std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())), YAML::Node>>,
        typename = void,
        typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        static_assert(sizeof...(args) == 0,
                      "Cannot pass additional args when using built-in YAML conversion");
        return std::make_optional<O>(_yaml.as<O>());
    }

    static YAML::Node parse(std::string);

    template <typename K>
    std::optional<const YAML::Node> yamlGet(const K& key) const {
        if (!_valid) {
            return std::nullopt;
        }
        const YAML::Node found = _yaml[key];
        if (found) {
            return std::make_optional<const YAML::Node>(found);
        } else {
            if (!_parent) {
                return std::nullopt;
            }
            return _parent->yamlGet(key);
        }
    }

    template <typename K>
    const Node get(const K& key) const {
        const std::string keyStr = v1::toString(key);
        if (!_valid) {
            return Node{YAML::Node{}, this, false, keyStr};
        }
        if constexpr (std::is_convertible_v<K, std::string>) {
            if (key == "..") {
                // this is...not the most succinct business-logic ever....
                std::stringstream childKey;
                if (_parent) {
                    childKey << _parent->_key << "/";
                }
                childKey << _key << "/..";
                if (!_parent) {
                    return Node{YAML::Node{}, nullptr, false, childKey.str()};
                }
                return Node{_parent->_yaml, _parent->_parent, _parent->_valid, childKey.str()};
            }
        }
        auto yaml = this->yamlGet(key);
        if (yaml) {
            return Node{*yaml, this, true, keyStr};
        } else {
            return Node{YAML::Node{}, this, false, keyStr};
        }
    }

    void appendKey(std::ostringstream& out) const;

    YAML::Node _yaml;
    std::string _key;
    const Node* _parent;
    bool _valid;
};


// we can act like a Node if iterated value is a scalar or
// we can act like a pair of Nodes if iterated value is a map entry
class IteratorValue : public std::pair<Node, Node>, public Node {
    using NodePair = std::pair<Node, Node>;

public:
    // jump through immense hoops to avoid knowing anything about the actual yaml iterator other
    // than its pair form is a pair of {YAML::Node, YAML::Node}
    template <typename ITVal>
    IteratorValue(const Node* parent, ITVal itVal, size_t index)
        : NodePair{std::make_pair(
              Node{itVal.first,
                   parent,
                   itVal.first,
                   itVal.first ? (itVal.first.template as<std::string>() + "$key") : ""},
              Node{itVal.second,
                   parent,
                   itVal.second,
                   itVal.first ? itVal.first.template as<std::string>() : v1::toString(index)})},
          Node{itVal, parent, itVal, v1::toString(index)} {}
};


class Node::iterator {
    // Don't expose or assume the type of YAML::Node.begin()
    using IterType = decltype(std::declval<const YAML::Node>().begin());

    IterType _child;
    const Node* parent;
    size_t index = 0;

    iterator(IterType child, const Node* parent) : _child(std::move(child)), parent(parent) {}

    friend Node;

public:
    auto operator++() {
        ++index;
        return _child.operator++();
    }

    auto operator*() const {
        auto out = _child.operator*();
        return IteratorValue{parent, out, index};
    }

    auto operator-> () const {
        auto out = _child.operator*();
        return IteratorValue{parent, out, index};
    }

    auto operator==(const iterator& rhs) const {
        return _child == rhs._child;
    }

    auto operator!=(const iterator& rhs) const {
        return _child != rhs._child;
    }
};

template <typename T, typename F>
std::vector<T> Node::getPlural(const std::string& singular,
                               const std::string& plural,
                               F&& f) const {
    std::vector<T> out;
    auto pluralValue = (*this)[plural];
    auto singValue = (*this)[singular];
    if (pluralValue && singValue) {
        std::stringstream str;
        str << "Can't have both '" << singular << "' and '" << plural << "'.";
        BOOST_THROW_EXCEPTION(InvalidPathException(str.str()));
    } else if (pluralValue) {
        if (!pluralValue.isSequence()) {
            std::stringstream str;
            str << "'" << plural << "' must be a sequence type.";
            BOOST_THROW_EXCEPTION(InvalidPathException(str.str()));
        }
        for (auto&& val : pluralValue) {
            T created = std::invoke(f, val);
            out.emplace_back(std::move(created));
        }
    } else if (singValue) {
        T created = std::invoke(f, singValue);
        out.emplace_back(std::move(created));
    } else if (!singValue && !pluralValue) {
        std::stringstream str;
        str << "Either '" << singular << "' or '" << plural << "' required.";
        BOOST_THROW_EXCEPTION(InvalidPathException(str.str()));
    }

    return out;
}

}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
