#pragma once
// TODO: proper header

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

namespace config {

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


template <typename T>
using BaseType = std::remove_reference<std::remove_cv_t<std::remove_pointer_t<T>>>;
/**
 * Is A same as B after removing all cv, ref, and ptr-ness
 */
template <typename A, typename B>
using IsLooselySame = typename std::is_same<typename BaseType<A>::type, typename BaseType<B>::type>;


class NodeT {
    const YAML::Node _yaml;
    const NodeT* const _parent;
    const bool _valid;

    // TODO: keep track of the key we came from so we can report it in error-messages
    // using KeyType = std::optional<std::variant<std::string, int>>;
    // KeyType _key;

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
        if (!_valid) {
            return NodeT{YAML::Node{}, this, false};
        }
        if constexpr (std::is_convertible_v<K, std::string>) {
            // this lets us avoid having to repeat parent key names
            // E.g. OperationName can now just be Name. If Actor
            // wants the Actor name they can look up `node[..][Name]`.
            if (key == "..") {
                if (!_parent) {
                    throw std::logic_error("TODO");  // TODO: better messaging
                }
                return *_parent;
            }
        }
        std::optional<const YAML::Node> yaml = this->yamlGet(key);
        if (yaml) {
            return NodeT{*yaml, this, true};
        } else {
            return NodeT{YAML::Node{}, this, false};
        }
    }

public:
    NodeT(const YAML::Node yaml, const NodeT* const parent, bool valid)
        : _yaml{yaml}, _parent{parent}, _valid{valid} {}

    explicit NodeT(const YAML::Node yaml) : NodeT{yaml, nullptr, (bool)yaml} {}

    // TODO: should this do fallback>?
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

    // TODO: should this do fallback?
    explicit operator bool() const {
        return _yaml && _valid;
    }

    template <typename O, typename... Args>
    O to(Args&&... args) const {
        return *maybe<O, Args...>(std::forward<Args>(args)...);
    }

    // synonym...do we actually want this? could allow for confusion...
    template<typename...Args>
    auto as(Args&&... args) const {
        return to(std::forward<Args>(args)...);
    }

    template <typename O, typename... Args>
    std::optional<O> maybe(Args&&... args) const {
        static_assert(!IsLooselySame<O, YAML::Node>::value, "🙈 YAML::Node");
        if (!*this) {
            return std::nullopt;
        }
        if constexpr (std::is_constructible_v<O, NodeT&, Args...> ||
                      std::is_constructible_v<O, const NodeT&, Args...>) {
            return std::make_optional<O>(*this, std::forward<Args>(args)...);
        } else {
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
};


// we can act like a NodeT if iterated value is a scalar or
// we can act like a pair of NodeTs if iterated value is a map entry
struct IteratorValue : public std::pair<NodeT, NodeT>, public NodeT {
    using NodePair = std::pair<NodeT, NodeT>;
    // jump through immense hoops to avoid knowing anything about the actual yaml iterator other
    // than its pair form is a pair of {YAML::Node, YAML::Node}
    template <typename ITVal>
    IteratorValue(const NodeT* parent, ITVal itVal)
        : NodePair{std::make_pair(NodeT{itVal.first, parent, itVal.first},
                                  NodeT{itVal.second, parent, itVal.second})},
          NodeT{itVal, parent, itVal} {}
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

    iterator(IterType child, const NodeT* parent) : _child(child), parent(parent) {}

    auto operator++() {
        return _child.operator++();
    }

    auto operator*() const {
        auto out = _child.operator*();
        return IteratorValue{parent, out};
    }

    auto operator-> () const {
        auto out = _child.operator*();
        return IteratorValue{parent, out};
    }

    auto operator==(const iterator& rhs) const {
        return _child == rhs._child;
    }

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

}  // namespace config
