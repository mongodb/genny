#pragma once
// TODO: proper header

#include <memory>
#include <optional>
#include <type_traits>
#include <variant>
#include <utility>

#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

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
    YAML::Node _yaml;
    NodeT* _parent;

    // TODO: keep track of the key we came from so we can report it in error-messages
    // using KeyType = std::optional<std::variant<std::string, int>>;
    // KeyType _key;

    template <typename K>
    std::optional<YAML::Node> yamlGet(const K& key) {
        YAML::Node found = _yaml[key];
        if (found) {
            return std::make_optional(found);
        } else {
            if (!_parent) {
                return std::nullopt;
            }
            return _parent->yamlGet(key);
        }
    }

    template <typename K>
    NodeT get(const K& key) {  // TODO: const version
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
        std::optional<YAML::Node> yaml = this->yamlGet(key);
        if (yaml) {
            return NodeT{*yaml, this};
        } else {
            throw std::logic_error("TODO");  // TODO: better messaging
        }
    }

public:
    NodeT(YAML::Node yaml, NodeT* parent)
            : _yaml{yaml}, _parent{parent} {}

    NodeT(YAML::Node yaml)
        : NodeT{yaml, nullptr} {}

    // TODO: this is a bad name
    template <typename O, typename... Args>
    O to(Args&&... args) { // TODO: const version
        // TODO: assert on remove_cv<O>
        static_assert(!std::is_same_v<O, YAML::Node>, "🙈 YAML::Node");
        // TODO: allow constructible with NodeT& being const
        // TODO: if sizeof...(Args) > 0 then this becomes a static_assert rather than an if
        //       (or put static_assert(sizeof == 0) in the else block)
        if constexpr (std::is_constructible_v<O, NodeT&,Args...>) {
            return O{*this, std::forward<Args>(args)...};
        } else {
            return NodeConvert<O>::convert(_yaml);
        }
    }

    template <typename K>
    NodeT operator[](const K& key) {  // TODO: const version
        return this->get(key);
    }
};


}  // namespace genny
