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

#include <stack>
#include <memory>

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

NodeType determineType(const YAML::Node node) {
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

class YamlKey {
public:
    explicit YamlKey(std::string key)
            : _key{key} {}
    explicit YamlKey(long key)
            : _key{key} {}
    const YAML::Node apply(const YAML::Node n) const {
        try {
            return n[std::get<std::string>(_key)];
        } catch(const std::bad_variant_access&) {
            return n[std::get<long>(_key)];
        }
    }
    bool isParent() const {
        try {
            return std::get<std::string>(_key) == "..";
        } catch(const std::bad_variant_access&) {
            return false;
        }
    }
    friend std::ostream& operator<<(std::ostream& out, const YamlKey& key) {
        try {
            out << std::get<std::string>(key._key);
        } catch(const std::bad_variant_access&) {
            out << std::get<long>(key._key);
        }
        return out;
    }
private:
    using type = std::variant<std::string, long>;
    type _key;
};


using Child = std::unique_ptr<NodeImpl>;
using ChildSequence = std::vector<Child>;
using ChildMap = std::map<std::string, Child>;

std::string appendPath(std::string path, std::string key) {
    return path + "/" + key;
}

}  // namespace


// NodeSource PiMPL
class NodeSourceImpl {
public:
    NodeSourceImpl(std::string yaml, std::string path)
    : _rootYaml(parse(yaml, path)), _path{path} {}

    Node root() const;
    YAML::Node rootYaml() const {
        return _rootYaml;
    }
private:
    YAML::Node _rootYaml;
    std::unique_ptr<const NodeImpl> _root;
    std::string _path;
};

class NodeImpl {
public:
    NodeImpl(const NodeImpl& parent, YamlKey key)
            : _source{parent._source}, _path{parent._path} {
        _path.push_back(key);
    }

    ~NodeImpl() = default;

    explicit operator bool() const {
        return bool(getNode());
    }

    std::string path() const;

    const YAML::Node getNode() const {
        std::stack<YAML::Node> out;

        out.push(_source->rootYaml());
        for(const auto& key : _path) {
            if (key.isParent()) {
                if (out.empty()) {
                    return YAML::Node{};
                }
                out.pop();
                continue;
            }
            out.push(key.apply(out.top()));
        }

        return out.top();
    }

    Node stringGet(std::string key) const {
        return Node{key, this};
    }

    Node longGet(long key) const {
        return Node{key, this};
    }

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
        return determineType(getNode());
    }

    size_t size() const {
        return this->getNode().size();
    }

    template <typename K>
    const NodeImpl* get(K&& key) const {
        if constexpr (std::is_convertible_v<K, std::string>) {
            return stringGet(std::forward<K>(key));
        } else {
            static_assert(std::is_convertible_v<K, size_t>);
            return longGet(std::forward<K>(key));
        }
    }

private:
    std::vector<YamlKey> _path;
    const NodeSourceImpl* _source;
};

std::string NodeImpl::path() const {
    std::stringstream out;
    for(const auto& key : _path) {
        out << key;
    }
    return out.str();
}

// NodeSource
NodeSource::NodeSource(std::string yaml, std::string path)
    : _impl{std::make_unique<NodeSourceImpl>(yaml, path)} {}

Node NodeSource::root() const {
    return _impl->root();
}

NodeSource::~NodeSource() = default;

Node NodeSourceImpl::root() const  {
    return Node{"", _root ? &*_root : nullptr};
}
// Node

bool Node::isScalar() const {
    if (!_impl) {
        return false;
    }
    return _impl->isScalar();
}

std::string Node::path() const {
    return _impl->path();
}

NodeType Node::type() const {
    return _impl->type();
}

bool Node::isSequence() const {
    return _impl->isSequence();
}

bool Node::isMap() const {
    return _impl->isMap();
}

bool Node::isNull() const {
    return _impl->isNull();
}

Node::operator bool() const {
    return _impl->operator bool();
}

Node Node::stringGet(std::string key) const {
    return _impl->stringGet(key);
}

Node Node::longGet(long key) const {
    return _impl->longGet(key);
}

size_t Node::size() const {
    return _impl->size();
}

YAML::Node Node::yaml() const {
    return _impl->getNode();
}

Node::Node(std::string key, const NodeImpl *parent)
        : _impl{std::make_unique<NodeImpl>(*parent, YamlKey{key})} {}

Node::Node(long key, const NodeImpl *parent)
        : _impl{std::make_unique<NodeImpl>(*parent, YamlKey{key})} {}

Node::~Node() = default;


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
//    out << "Invalid key 'key" << node->key() << "': ";
    out << msg << " ";
    out << "On node with path '" << node->path() << "': ";
//    out << *node;
    _what = out.str();
}

}  // namespace genny
