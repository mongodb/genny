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

template<typename O>
struct NodeConvert{
    static O convert(YAML::Node node) {
        return node.as<O>();
    }
};

struct WLContext {
    int rng() {
        return 7;
    }
};

class Node {
    YAML::Node _yaml;
    Node* _parent;
    WLContext* _context;

    using KeyType = std::optional<std::variant<std::string, int>>;
    KeyType _key;

    Node(YAML::Node yaml, Node* parent, WLContext* context)
    : _yaml{yaml}, _parent{parent}, _context{context} {}

    template<typename K>
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

    template<typename K>
    Node get(const K& key) { // TODO: const version
        // TODO: if key == ".." then look up in parent
        std::optional<YAML::Node> yaml = this->yamlGet(key);
        if (yaml) {
            return Node{*yaml, this, _context};
        } else {
            BOOST_LOG_TRIVIAL(info) << "Key " << key << " not found";
            throw std::logic_error("TODO"); // TODO: better messaging
        }
    }

public:
    Node(YAML::Node topLevel, WLContext* context)
    : Node{topLevel, nullptr, context} {}

    template<typename O>
    O as() { // TODO: const version
        static_assert(!std::is_same_v<O, YAML::Node>, "🙈 YAML::Node");
        if constexpr( std::is_constructible_v<O,Node&,WLContext*> ) {
            return O{*this, this->_context};
        } else {
            return NodeConvert<O>::convert(_yaml);
        }
    }

    template<typename K>
    Node operator[](const K& key) { // TODO: const version
        return this->get(key);
    }
};




}

