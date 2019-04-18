#include <utility>

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

#pragma once
// TODO: proper header

#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

template <typename O>
struct NodeConvert {
    static O convert(YAML::Node node) {
        return node.as<O>();
    }
};

template <typename C>
class NodeT {
    YAML::Node _yaml;
    NodeT* _parent;
    C* _context;

    using KeyType = std::optional<std::variant<std::string, int>>;
    KeyType _key;

    NodeT(YAML::Node yaml, NodeT* parent, C* context)
        : _yaml{yaml}, _parent{parent}, _context{context} {}

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
            if (key == "..") {
                if (!_parent) {
                    BOOST_LOG_TRIVIAL(info) << "No parent in node= " << YAML::Dump(_yaml);
                    throw std::logic_error("TODO");  // TODO: better messaging
                }
                return *_parent;
            }
        }
        std::optional<YAML::Node> yaml = this->yamlGet(key);
        if (yaml) {
            return NodeT{*yaml, this, _context};
        } else {
            BOOST_LOG_TRIVIAL(info) << "Key " << key << " not found";
            throw std::logic_error("TODO");  // TODO: better messaging
        }
    }

public:
    NodeT(YAML::Node topLevel, C* context) : NodeT{topLevel, nullptr, context} {}

    template <typename O>
    O as() {  // TODO: const version
        return this->from<O>();
    }

    template <typename O, typename... Args>
    O from(Args&&... args) {
        static_assert(!std::is_same_v<O, YAML::Node>, "🙈 YAML::Node");
        if constexpr (std::is_constructible_v<O, NodeT&, C*, Args...>) {
            return O{*this, this->_context, std::forward<Args>(args)...};
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
