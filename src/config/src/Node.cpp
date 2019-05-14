#include <config/Node.hpp>
#include <map>

namespace {

using Child = std::unique_ptr<class Node>;
using Children = std::map<YamlKey, Child>;

}  // namespace

class NodeImpl {
public:
    explicit NodeImpl(const Node* self, YamlKey path, YAML::Node yaml)
        : _children{constructChildren(self, yaml)}, _yaml{yaml}, _path{path}, _self{self} {}

    const Node& get(YamlKey key) const {
        auto&& it = _children.find(key);
        if (it == _children.end()) {
            _children.emplace(key, std::make_unique<Node>(_self, key, _zombie));
        }
        return *_children.at(key);
    }

    const YAML::Node yaml() const {
        return _yaml;
    }

    explicit operator bool() const {
        return bool(_yaml);
    }

    bool isMap() const {
        return _yaml.IsMap();
    }

    bool isSequence() const {
        return _yaml.IsSequence();
    }

    bool isNull() const {
        return _yaml.IsNull();
    }

    size_t size() const {
        return _yaml.size();
    }

    std::string path() const {
        return _path.toString();
    }

    friend std::ostream& operator<<(std::ostream& out, const NodeImpl& impl) {
        return out << YAML::Dump(impl._yaml);
    }

private:
    static YAML::Node _zombie;
    friend iterator;

    // Needs to be mutable to generate placeholder nodes for non-existent keys.
    mutable Children _children;
    YAML::Node _yaml;
    YamlKey _path;
    const Node* _self;

    static Children constructChildren(const Node *parent, YAML::Node node) {
        Children out;
        if (!node.IsMap() && !node.IsSequence()) {
            return out;
        }

        int index = 0;
        auto extractKey = [&](auto& kvp, YAML::Node iteratee) -> YamlKey {
            if (iteratee.IsMap()) {
                return YamlKey{kvp.first.template as<std::string>()};
            } else {
                return YamlKey{index++};
            }
        };
        auto extractValue = [&](auto& kvp, YAML::Node iteratee) -> YAML::Node {
            if (iteratee.IsMap()) {
                return kvp.second;
            } else {
                return kvp;
            }
        };

        for (const auto kvp: node) {
            const auto key = extractKey(kvp, node);
            out.emplace(key, std::make_unique<Node>(parent, key, extractValue(kvp, node)));
        }

        return out;
    }
};

std::string Node::path() const {
    return this->_impl->path();
}

class iterator Node::begin() const {
    return iterator{&*this->_impl, false};
}
class iterator Node::end() const {
    return iterator{&*this->_impl, true};
}

std::ostream& operator<<(std::ostream& out, const Node& node) {
    return out << *(node._impl);
}

std::ostream& operator<<(std::ostream& out, const YamlKey& key)  {
    try {
        return out << std::get<std::string>(key._value);
    } catch(const std::bad_variant_access&) {
        return out << std::get<long>(key._value);
    }
}
std::string YamlKey::toString() const  {
    std::stringstream out;
    out << *this;
    return out.str();
}

bool Node::isNull() const {
    return _impl->isNull();
}

bool Node::isMap() const {
    return _impl->isMap();
}
bool Node::isSequence() const {
    return _impl->isSequence();
}
size_t Node::size() const {
    return _impl->size();
}

YAML::Node NodeImpl::_zombie = YAML::Load("")["zombie"];

const YAML::Node Node::yaml() const {
    return _impl->yaml();
}

Node::operator bool() const {
    return _impl->operator bool();
}

Node::~Node() = default;

// TODO: parent unused
Node::Node(const Node* parent, YamlKey path, YAML::Node yaml)
    : _impl{std::make_unique<NodeImpl>(this, path, yaml)} {}

const Node& Node::operator[](long key) const {
    return _impl->get(YamlKey{key});
}

const Node& Node::operator[](std::string key) const {
    return _impl->get(YamlKey{key});
}

NodeSource::NodeSource(std::string yaml, std::string path)
: _yaml{YAML::Load(yaml)},
  _root{std::make_unique<Node>(nullptr, YamlKey{""}, _yaml)},
  _path{path} {}

NodeSource::~NodeSource() = default;

template<typename T>
class TD;

class IteratorImpl {
public:
    void increment() {
        ++_children;
    }
    bool equal(const IteratorImpl& rhs) const {
        return _children == rhs._children;
    }
    bool notEqual(const IteratorImpl& rhs) const {
        return _children != rhs._children;
    }
    const iterator_value getValue() const {
        auto& [key, implPtr] = *_children;
        return {key, *implPtr};
    }

    IteratorImpl(Children::const_iterator children)
            : _children(children) {}

private:
    Children::const_iterator _children;
};

void iterator::operator++() {
    return _impl->increment();
}

bool iterator::operator!=(const iterator& rhs) const {
    return _impl->notEqual(*rhs._impl);
}
bool iterator::operator==(const iterator &rhs) const {
    return _impl->equal(*rhs._impl);
}

const iterator_value iterator::operator*() const {
    return _impl->getValue();
}

iterator::iterator(const NodeImpl* nodeImpl, bool end)
: _impl{std::make_unique<IteratorImpl>(end ? nodeImpl->_children.end() : nodeImpl->_children.begin())}
{}




iterator::~iterator() = default;

iterator_value::iterator_value(const YamlKey key, const Node &node)
: std::pair<const YamlKey, const Node&>{key, node} {}

