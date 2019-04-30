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

#include <boost/log/trivial.hpp>

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

class Node {

private:
    Node(const YAML::Node yaml, const Node* const parent, bool valid, std::string key)
        : _yaml{yaml}, _parent{parent}, _valid{valid}, _key{std::move(key)} {}

    Node(const YAML::Node yaml, const Node* const parent, std::string key)
        : Node{yaml, parent, yaml, std::move(key)} {}

    // <yikes>
    // TODO: static assert that at least one of these works out in the regular maybe impl
    // to make the error-messages not a disaster

    template <typename O, typename... Args>
    static constexpr bool isNodeConstructible() {
        // TODO: test of constness
        return std::is_constructible_v<O, Node&, Args...> ||
            std::is_constructible_v<O, const Node&, Args...>;
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<isNodeConstructible<O, Args...>()>>
    std::optional<O> _maybeImpl(Args&&... args) const {
        return std::make_optional<O>(*this, std::forward<Args>(args)...);
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<!isNodeConstructible<O, Args...>() &&
                                          std::is_same_v<O, typename NodeConvert<O>::type> &&
                                          std::is_same_v<O, decltype(NodeConvert<O>::convert(std::declval<Node>(),std::declval<Args>()...))>>,
              typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        return std::make_optional<O>(NodeConvert<O>::convert(*this, std::forward<Args>(args)...));
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<
                  !isNodeConstructible<O, Args...>() &&
                  // is there a better way to do this?
                  std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())), YAML::Node>>,
              typename = void,
              typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        static_assert(sizeof...(args) == 0,
                      "Cannot pass additional args when using built-in YAML conversion");
        return std::make_optional<O>(_yaml.as<O>());
    }
    // </yikes>


    static YAML::Node parse(std::string);

public:
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
        if (!_valid) {
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

    template <typename O, typename... Args>
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

    std::string path() const;

    friend class IteratorValue;

    struct iterator;
    iterator begin() const;
    iterator end() const;

private:
    YAML::Node _yaml;
    std::string _key;
    const Node* _parent;
    bool _valid;

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
};


// we can act like a Node if iterated value is a scalar or
// we can act like a pair of Nodes if iterated value is a map entry
struct IteratorValue : public std::pair<Node, Node>, public Node {
    using NodePair = std::pair<Node, Node>;
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


struct Node::iterator {
    // Don't expose or assume the type of YAML::Node.begin()
    using IterType = decltype(std::declval<const YAML::Node>().begin());

    IterType _child;
    const Node* parent;
    size_t index = 0;

    iterator(IterType child, const Node* parent) : _child(std::move(child)), parent(parent) {}

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

inline Node::iterator Node::begin() const {
    return Node::iterator{_yaml.begin(), this};
}

inline Node::iterator Node::end() const {
    return Node::iterator{_yaml.end(), this};
}

}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
