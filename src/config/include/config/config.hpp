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

// template<typename T>
// class ValueOrHolder {
//    // TODO: also wire in path and barf if try to access via operatorT() and _value is nullopt
//    std::optional<T> _value;
//
// public:
//    auto operator->() const {
//        return _value.operator->();
//    }
//    operator T() const {
//        return *_value;
//    }
//    template <typename R>
//    bool operator==(R&& r) {
//        return _value.operator==(std::forward<R>(r));
//    }
//    template<typename...Args>
//    T value_or(Args&&...args) {
//        if (_value) {
//            return *_value;
//        } else {
//            return T{std::forward<Args>(args)...};
//        }
//    }
//};

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

    // TODO: this is a bad name
    template <typename O, typename... Args>
    O to(Args&&... args) const {
        return *maybe<O, Args...>(std::forward<Args>(args)...);
    }

    template <typename O, typename... Args>
    std::optional<O> maybe(Args&&... args) const {
        if (*this) {
            // TODO: assert on remove_cv<O>
            static_assert(!std::is_same_v<O, YAML::Node>, "🙈 YAML::Node");
            // TODO: allow constructible with NodeT& being const
            // TODO: if sizeof...(Args) > 0 then this becomes a static_assert rather than an if
            //       (or put static_assert(sizeof == 0) in the else block)
            if constexpr (std::is_constructible_v<O, NodeT&, Args...>) {
                return std::make_optional<O>(*this, std::forward<Args>(args)...);
            } else {
                return std::make_optional<O>(NodeConvert<O>::convert(_yaml));
            }
        } else {
            return std::nullopt;
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

template<class T>
class TD;

struct IteratorValue : std::pair<NodeT,NodeT> {
    using NodePair = std::pair<NodeT,NodeT>;
    using NodePair::NodePair;
};

struct NodeT::iterator {
    YAML::const_iterator _child;
    const NodeT* parent;

    auto operator++() {
        return _child.operator++();
    }

    auto operator*() {
        auto out = _child.operator*();
        return IteratorValue{NodeT{out.first, parent, out.first}, NodeT{out.second, parent, out.second}};
    }

    auto operator->() {
        auto out = _child.operator*();
        return IteratorValue{NodeT{out.first, parent, out.first}, NodeT{out.second, parent, out.second}};
    }

    auto operator==(const iterator &rhs) {
        return _child == rhs._child;
    }

    auto operator!=(const iterator &rhs) {
        return _child != rhs._child;
    }
};

inline NodeT::iterator NodeT::begin() const {
    return NodeT::iterator{_yaml.begin()};
}

inline NodeT::iterator NodeT::end() const {
    return NodeT::iterator{_yaml.end()};
}

// TODO: rename NodeT to just node...not templated on ptr type anymore
using Node = NodeT;

}  // namespace config
