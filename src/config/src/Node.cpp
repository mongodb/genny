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

}  // namespace

NodeSource::NodeSource(std::string yaml, std::string path)
: _root{std::make_unique<NodeImpl>(parse(yaml, path), nullptr)},
  _path{std::move(path)} {}

Node NodeSource::root() const  {
    return {&*_root, _path};
}

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

size_t NodeImpl::size() const {
    if (isMap()) {
        return _childMap.size();
    } else if (isSequence()) {
        return _childSequence.size();
    } else {
        return 0;
    }
}

const NodeImpl* NodeImpl::stringGet(const std::string &key) const {
    if (!isMap()) {
        return nullptr;
    }
    if(const auto& found = _childMap.find(key); found != _childMap.end()) {
        return &*(found->second);
    } else {
        return nullptr;
    }
}

const NodeImpl* NodeImpl::longGet(long key) const {
    if (key < 0 || key >= _childSequence.size() || !isSequence()) {
        return nullptr;
    }
    const auto& child = _childSequence.at(key);
    return &*(child);
}

NodeImpl::NodeImpl(YAML::Node node, const NodeImpl *parent)    : _node{node},
                                                                 _parent{parent},
                                                                 _nodeType{determineType(_node)},
                                                                 _childSequence(childSequence(node, this)),
                                                                 _childMap(childMap(node, this)) {}

bool NodeImpl::isNull() const  {
    return type() == NodeType::Null;
}

NodeType NodeImpl::determineType(YAML::Node node)  {
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

NodeImpl::ChildMap NodeImpl::childMap(YAML::Node node, const NodeImpl *parent)  {
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

NodeImpl::ChildSequence NodeImpl::childSequence(YAML::Node node, const NodeImpl *parent)  {
    ChildSequence out;
    if (node.Type() != YAML::NodeType::Sequence) {
        return out;
    }
    for(const auto& kvp : node) {
        out.emplace_back(std::make_unique<NodeImpl>(kvp, parent));
    }
    return out;
}

const NodeImpl &NodeImpl::childSequenceGet(const long key) const  {
    if (!isSequence()) {
        // TODO: handle bad type
        BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
    }
    assert(_nodeType == NodeType::Sequence);
    return *(_childSequence.at(key));
    // TODO: handle std::out_of_range
}

const NodeImpl &NodeImpl::childMapGet(const std::string &key) const  {
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

bool NodeImpl::isScalar() const  {
    return type() == NodeType::Scalar;
}

bool NodeImpl::isSequence() const  {
    return type() == NodeType::Sequence;
}

bool NodeImpl::isMap() const {
    return type() == NodeType::Map;
}

NodeType NodeImpl::type() const  {
    return _nodeType;
}


}  // namespace genny
