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

std::string InvalidYAMLException::createWhat(const std::string& path,
                                             const YAML::ParserException& yamlException) {
    std::stringstream out;
    out << "Invalid YAML: ";
    out << "'" << yamlException.msg << "' ";
    out << "at (Line:Column)=(" << yamlException.mark.line << ":" << yamlException.mark.column
        << "). ";
    out << "On node with path '" << path << "'.";

    return out.str();
}
//
//std::string InvalidConversionException::createWhat(const Node& node,
//                                                   const YAML::BadConversion& yamlException,
//                                                   const std::type_info& destType) {
//    std::stringstream out;
//    out << "Couldn't convert to '" << boost::core::demangle(destType.name()) << "': ";
//    out << "'" << yamlException.msg << "' at (Line:Column)=(" << yamlException.mark.line << ":"
//        << yamlException.mark.column << "). ";
//    out << "On node with path '" << node.path() << "': ";
//    out << node;
//
//    return out.str();
//}
//
//std::string InvalidKeyException::createWhat(const std::string& msg,
//                                            const std::string& key,
//                                            const Node* node) {
//    std::stringstream out;
//    out << "Invalid key '" << key << "': ";
//    out << msg << " ";
//    out << "On node with path '" << node->path() << "': ";
//    out << *node;
//
//    return out.str();
//}
//
namespace {

// Helper to parse yaml string and throw a useful error message if parsing fails
YAML::Node parse(std::string yaml, const std::string& path) {
    try {
        return YAML::Load(yaml);
    } catch (const YAML::ParserException& x) {
        BOOST_THROW_EXCEPTION(InvalidYAMLException(path, x));
    }
}

NodeType determineType(YAML::Node node)  {
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

using Child = std::unique_ptr<NodeImpl>;
using ChildSequence = std::vector<Child>;
using ChildMap = std::map<std::string, Child>;

}  // namespace


// Always owned by NodeSource (below)
class NodeImpl {
public:
    NodeImpl(YAML::Node node, const NodeImpl* parent)
            : _node{node},
              _parent{parent},
              _nodeType{determineType(_node)},
              _childSequence(childSequence(node, this)),
              _childMap(childMap(node, this)) {}

    bool isNull() const  {
        return type() == NodeType::Null;
    }

    bool isScalar() const  {
        return type() == NodeType::Scalar;
    }

    bool isSequence() const  {
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

    template<typename K>
    const NodeImpl& get(K&& key) const {
        if constexpr (std::is_convertible_v<K, std::string>) {
            return childMapGet(std::forward<K>(key));
        } else {
            static_assert(std::is_constructible_v<K, size_t>);
            return childSequenceGet(std::forward<K>(key));
        }
    }

    const NodeImpl* stringGet(const std::string &key) const {
        if (!isMap()) {
            return nullptr;
        }
        if(const auto& found = _childMap.find(key); found != _childMap.end()) {
            return &*(found->second);
        } else {
            return nullptr;
        }
    }

    const NodeImpl* longGet(long key) const{
        if (key < 0 || key >= _childSequence.size() || !isSequence()) {
            return nullptr;
        }
        const auto& child = _childSequence.at(key);
        return &*(child);
    }

private:
    const YAML::Node _node;
    const NodeImpl* _parent;
    const NodeType _nodeType;

    const ChildSequence _childSequence;
    const ChildMap _childMap;

    const NodeImpl& childMapGet(const std::string &key) const  {
        if (!isMap()) {
            // TODO: handle bad type
            BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
        }
        if (auto found = _childMap.find(key); found != _childMap.end()) {
            return *(found->second);
        }
        // TODO: handle out of range
        BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
    }

    const NodeImpl& childSequenceGet(const long key) const  {
        if (!isSequence()) {
            // TODO: handle bad type
            BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
        }
        assert(_nodeType == NodeType::Sequence);
        return *(_childSequence.at(key));
        // TODO: handle std::out_of_range
    }

    static ChildSequence childSequence(YAML::Node node, const NodeImpl *parent)  {
        ChildSequence out;
        if (node.Type() != YAML::NodeType::Sequence) {
            return out;
        }
        for(const auto& kvp : node) {
            out.emplace_back(std::make_unique<NodeImpl>(kvp, parent));
        }
        return out;
    }

    static ChildMap childMap(YAML::Node node, const NodeImpl *parent)  {
        ChildMap out;
        if (node.Type() != YAML::NodeType::Map) {
            return out;
        }
        for(const auto& kvp : node) {
            auto childKey = kvp.first.as<std::string>();
            out.emplace(childKey, std::make_unique<NodeImpl>(kvp.second, parent));
        }
        return out;
    }
};

NodeSource::NodeSource(std::string yaml, std::string path)
: _root{std::make_unique<NodeImpl>(parse(yaml, path), nullptr)},
  _path{std::move(path)} {}

Node NodeSource::root() const  {
    return {&*_root, _path};
}

NodeSource::~NodeSource() = default;

bool Node::isScalar() const {
    if (!_impl) {
        return false;
    }
    return _impl->isScalar();
}

NodeType Node::type() const {
    if (!_impl) {
        return NodeType::Undefined;
    }
    return _impl->type();
}

bool Node::isSequence() const {
    if (!_impl) {
        return false;
    }
    return _impl->isSequence();
}

bool Node::isMap() const {
    if (!_impl) {
        return false;
    }
    return _impl->isMap();
}

bool Node::isNull() const {
    if (!_impl) {
        return false;
    }
    return _impl->isNull();
}

Node::operator bool() const {
    auto out = bool(_impl);
    return out;
}
//
//std::string Node::path() const {
//    std::stringstream out;
//    this->buildPath(out);
//    return out.str();
//}
//
//Node::iterator Node::begin() const {
//    return Node::iterator{_yaml.begin(), this};
//}
//
//Node::iterator Node::end() const {
//    return Node::iterator{_yaml.end(), this};
//}
//
//
Node Node::stringGet(std::string key) const {
    if (!_impl) {
        return {nullptr, key};
    }
    const NodeImpl* childImpl = _impl->stringGet(key);
    // TODO: append path better
    return {childImpl, key};
}

Node Node::longGet(long key) const {
    std::string keyStr = std::to_string(key);
    if (!_impl) {
        return {nullptr, keyStr};
    }

    const NodeImpl* childImpl = _impl->longGet(key);
    // TODO: append path better
    return {childImpl, keyStr};
}

size_t Node::size() const {
    if (!_impl) {
        return 0;
    }
    return _impl->size();
}

Node::Node(const NodeImpl *impl, std::string path)
        : _impl{impl}, _path{path} {}

}  // namespace genny
