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
struct NodeConvert {
    static O convert(YAML::Node node) {
        return node.as<O>();
    }
};

class NodeT {
public:
    NodeT(const YAML::Node yaml, std::string key)
        : NodeT{yaml, nullptr, (bool)yaml, std::move(key)} {}

    NodeT(const YAML::Node yaml, const NodeT* const parent, bool valid, std::string key)
        : _yaml{yaml}, _parent{parent}, _valid{valid}, _key{std::move(key)} {}

    std::string path() const {
        std::ostringstream out;
        this->appendKey(out);
        return out.str();
    }

    // TODO: this syntax-sugar could be too confusing versus when you should
    // just do node.maybe<T>().value_or(T{})
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

    auto size() const {
        return _yaml.size();
    }

    explicit operator bool() const {
        return _valid && _yaml;
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

    // synonym...do we actually want this? could allow for confusion...
    template <typename... Args>
    auto as(Args&&... args) const {
        return to(std::forward<Args>(args)...);
    }

    template <typename O, typename... Args>
    std::optional<O> maybe(Args&&... args) const {
        // TODO: tests of this
        static_assert(!std::is_same_v<std::decay_t<YAML::Node>, std::decay_t<O>>, "🙈 YAML::Node");
        if (!*this) {
            return std::nullopt;
        }
        // TODO: tests of the const-ness
        if constexpr (std::is_constructible_v<O, NodeT&, Args...> ||
                      std::is_constructible_v<O, const NodeT&, Args...>) {
            return std::make_optional<O>(*this, std::forward<Args>(args)...);
        } else {
            // TODO: tests the sizeof failure-case
            static_assert(sizeof...(args) == 0, "Must be constructible from Node& and given args");
            return std::make_optional<O>(NodeConvert<O>::convert(_yaml));
        }
    }

    template <typename K>
    const NodeT operator[](const K& key) const {
        return this->get(key);
    }

    struct iterator;
    iterator begin() const;
    iterator end() const;

private:
    const YAML::Node _yaml;
    const std::string _key;
    const NodeT* const _parent;
    const bool _valid;

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
    const NodeT get(const K& key) const {
        const std::string keyStr = toString(key);
        if (!_valid) {
            return NodeT{YAML::Node{}, this, false, keyStr};
        }
        if constexpr (std::is_convertible_v<K, std::string>) {
            // this lets us avoid having to repeat parent key names
            // E.g. OperationName can now just be Name. If Actor
            // wants the Actor name they can look up `node[..][Name]`.
            // TOOD: test that we can [..] past the root element by a bunch and not blow up
            if (key == "..") {
                // this is...not the most succinct business-logic ever....
                std::stringstream childKey;
                if (_parent) {
                    childKey << _parent->_key << "/";
                }
                childKey << _key << "/..";
                if (!_parent) {
                    return NodeT{YAML::Node{}, nullptr, false, childKey.str()};
                }
                return NodeT{_parent->_yaml, _parent->_parent, _parent->_valid, childKey.str()};
            }
        }
        std::optional<const YAML::Node> yaml = this->yamlGet(key);
        if (yaml) {
            return NodeT{*yaml, this, true, keyStr};
        } else {
            return NodeT{YAML::Node{}, this, false, keyStr};
        }
    }

    void appendKey(std::ostringstream& out) const {
        if (_parent) {
            _parent->appendKey(out);
            out << "/";
        }
        out << _key;
    }
};


// we can act like a NodeT if iterated value is a scalar or
// we can act like a pair of NodeTs if iterated value is a map entry
struct IteratorValue : public std::pair<NodeT, NodeT>, public NodeT {
    using NodePair = std::pair<NodeT, NodeT>;
    // jump through immense hoops to avoid knowing anything about the actual yaml iterator other
    // than its pair form is a pair of {YAML::Node, YAML::Node}
    template <typename ITVal>
    IteratorValue(const NodeT* parent, ITVal itVal, size_t index)
        : NodePair{std::make_pair(
              NodeT{itVal.first,
                    parent,
                    itVal.first,
                    itVal.first ? (itVal.first.template as<std::string>() + "$key") : ""},
              NodeT{itVal.second,
                    parent,
                    itVal.second,
                    itVal.first ? itVal.first.template as<std::string>() : v1::toString(index)})},
          NodeT{itVal, parent, itVal, v1::toString(index)} {}
};

// more hoops to avoid hard-coding to YAML::Node internals
auto iterType() -> decltype(auto) {
    const YAML::Node node;
    return node.begin();
}

using IterType = decltype(iterType());

struct NodeT::iterator {
    IterType _child;
    const NodeT* parent;
    size_t index = 0;

    iterator(IterType child, const NodeT* parent) : _child(std::move(child)), parent(parent) {}

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

    // TODO: just forward directly
    auto operator==(const iterator& rhs) const {
        return _child == rhs._child;
    }

    // TODO: just forward directly
    auto operator!=(const iterator& rhs) const {
        return _child != rhs._child;
    }
};

inline NodeT::iterator NodeT::begin() const {
    return NodeT::iterator{_yaml.begin(), this};
}

inline NodeT::iterator NodeT::end() const {
    return NodeT::iterator{_yaml.end(), this};
}

// TODO: rename NodeT to just node...not templated on ptr type anymore
using Node = NodeT;

}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
