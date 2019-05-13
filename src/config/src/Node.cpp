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

#include <config/Node.hpp>

#include <boost/core/demangle.hpp>

namespace genny {

namespace {

// Helper to parse yaml string and throw a useful error message if parsing fails
YAML::Node parse(std::string yaml, const std::string& path) {
    try {
        return YAML::Load(yaml);
    } catch (const YAML::ParserException& x) {
        BOOST_THROW_EXCEPTION(InvalidYAMLException(path, x));
    }
}

NodeType determineType(YAML::Node node) {
    auto yamlTyp = node.Type();
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

using Child = std::unique_ptr<BaseNodeImpl>;
using ChildSequence = std::vector<Child>;
using ChildMap = std::map<std::string, Child>;

std::string appendPath(std::string path, std::string key) {
    return path + "/" + key;
}

}  // namespace


class NodeFields {
public:
    explicit NodeFields(const BaseNodeImpl* self, const BaseNodeImpl* parent)
        : _self{self},
          _parent{parent},
          _nodeType{determineType(self->node)},
          _childSequence(childSequence(self->node, self)),
          _childMap(childMap(self->node, self)) {}

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
        return _nodeType;
    }

    size_t size() const {
        if (isMap()) {
            return _childMap.size();
        } else if (isSequence()) {
            return _childSequence.size();
        } else {
            return 0;
        }
    }

    template <typename K>
    const BaseNodeImpl* get(K&& key) const {
        const BaseNodeImpl* out = nullptr;
        if constexpr (std::is_convertible_v<K, std::string>) {
            out = stringGet(std::forward<K>(key));
        } else {
            static_assert(std::is_convertible_v<K, size_t>);
            out = longGet(std::forward<K>(key));
        }
        if (out != nullptr) {
            return out;
        } else if (_parent == nullptr) {
            return nullptr;
        }
        return _parent->rest->get(key);
    }

    const BaseNodeImpl* _self;
    const BaseNodeImpl* _parent;

private:

    const NodeType _nodeType;
    const ChildSequence _childSequence;
    const ChildMap _childMap;

    const BaseNodeImpl* stringGet(const std::string& key) const {
        if (const auto& found = _childMap.find(key); found != _childMap.end()) {
            return &*(found->second);
        }
        return nullptr;
    }

    const BaseNodeImpl* longGet(long key) const {
        if (key < 0 || key >= _childSequence.size()) {
            return nullptr;
        }
        const auto& child = _childSequence.at(key);
        return &*(child);
    }

    static ChildSequence childSequence(const YAML::Node node, const BaseNodeImpl* self) {
        ChildSequence out;
        if (node.Type() != YAML::NodeType::Sequence) {
            return out;
        }
        for (YAML::Node kvp : node) {
            out.emplace_back(std::make_unique<BaseNodeImpl>(kvp, self));
        }
        return out;
    }

    static ChildMap childMap(YAML::Node node, const BaseNodeImpl* self) {
        ChildMap out;
        if (node.Type() != YAML::NodeType::Map) {
            return out;
        }
        for (const auto& kvp : node) {
            auto childKey = kvp.first.as<std::string>();
            out.emplace(childKey, std::make_unique<BaseNodeImpl>(kvp.second, self));
        }
        return out;
    }
};

// NodeSource

NodeSource::NodeSource(std::string yaml, std::string path)
    : _root{std::make_unique<BaseNodeImpl>(parse(yaml, path), nullptr)},
      _path{std::move(path)} {}

Node NodeSource::root() const {
    return {&*_root, _path, ""};
}

NodeSource::~NodeSource() = default;

// Node

bool Node::isScalar() const {
    if (!_impl) {
        return false;
    }
    return _impl->rest->isScalar();
}

std::string Node::path() const {
    return _path;
}

std::string Node::key() const {
    return _key;
}

NodeType Node::type() const {
    if (!_impl) {
        return NodeType::Undefined;
    }
    return _impl->rest->type();
}

bool Node::isSequence() const {
    if (!_impl) {
        return false;
    }
    return _impl->rest->isSequence();
}

bool Node::isMap() const {
    if (!_impl) {
        return false;
    }
    return _impl->rest->isMap();
}

bool Node::isNull() const {
    if (!_impl) {
        return false;
    }
    return _impl->rest->isNull();
}

Node::operator bool() const {
    return bool(_impl);
}

Node Node::stringGet(std::string key) const {
    if (!_impl) {
        return {nullptr, appendPath(_path, key), key};
    }
    const BaseNodeImpl* childImpl = _impl->rest->get(key);
    return {childImpl, appendPath(_path, key), key};
}

Node Node::longGet(long key) const {
    std::string keyStr = std::to_string(key);
    if (!_impl) {
        return {nullptr, appendPath(_path, keyStr), keyStr};
    }

    const BaseNodeImpl* childImpl = _impl->rest->get(key);
    return {childImpl, appendPath(_path, keyStr), keyStr};
}

size_t Node::size() const {
    if (!_impl) {
        return 0;
    }
    return _impl->rest->size();
}

Node::Node(const BaseNodeImpl* impl, std::string path, std::string key) : _impl{impl}, _path{path}, _key{key} {}


//
// std::string Node::path() const {
//    std::stringstream out;
//    this->buildPath(out);
//    return out.str();
//}
//
// Node::iterator Node::begin() const {
//    return Node::iterator{_yaml.begin(), this};
//}
//
// Node::iterator Node::end() const {
//    return Node::iterator{_yaml.end(), this};
//}
//
//

// Exception Types

namespace {
std::string createInvalidYamlExceptionWhat(const std::string& path,
                                           const YAML::ParserException& yamlException) {
    std::stringstream out;
    out << "Invalid YAML: ";
    out << "'" << yamlException.msg << "' ";
    out << "at (Line:Column)=(" << yamlException.mark.line << ":" << yamlException.mark.column
        << "). ";
    out << "On node with path '" << path << "'.";

    return out.str();
}
}

InvalidYAMLException::InvalidYAMLException(const std::string &path, const YAML::ParserException &yamlException)
        : _what{createInvalidYamlExceptionWhat(path, yamlException)} {}

InvalidConversionException::InvalidConversionException(const struct Node *node,
                                                       const YAML::BadConversion &yamlException,
                                                       const std::type_info &destType) {
    std::stringstream out;
    out << "Couldn't convert to '" << boost::core::demangle(destType.name()) << "': ";
    out << "'" << yamlException.msg << "' at (Line:Column)=(" << yamlException.mark.line << ":"
        << yamlException.mark.column << "). ";
    out << "On node with path '" << node->path() << "': ";
    out << *node;
    this->_what = out.str();
}

InvalidKeyException::InvalidKeyException(const std::string &msg, const genny::Node* node) {
    std::stringstream out;
    out << "Invalid key '" << node->key() << "': ";
    out << msg << " ";
    out << "On node with path '" << node->path() << "': ";
//    out << *node;
    _what = out.str();
}

BaseNodeImpl::BaseNodeImpl(YAML::Node node, const BaseNodeImpl* parent)
: node{node}, rest{std::make_unique<NodeFields>(this, parent)} {}


}  // namespace genny
