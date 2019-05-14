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

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <yaml-cpp/yaml.h>


class Node {
public:
    const Node& operator[](long key) const;
    const Node& operator[](std::string key) const;
    ~Node();

private:
    friend class NodeImpl;
    friend class NodeSource;
    std::unique_ptr<class NodeImpl> _impl;
public:
    explicit Node(const Node* parent, long key, YAML::Node yaml);
    explicit Node(const Node* parent, std::string key, YAML::Node yaml);
};

class NodeSource {
public:
    const Node& root() const {
        return *_root;
    }
    NodeSource(std::string yaml, std::string path);
private:
    YAML::Node _yaml;
    std::unique_ptr<Node> _root;
    std::string _path;
};


#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
