#include <config/Node.hpp>

namespace {
using Child = std::unique_ptr<class Node>;
using ChildrenMap = std::map<std::string, Child>;
using ChildrenSeq = std::vector<Child>;

class YamlKey {
public:
    explicit YamlKey(std::string key)
    : _value{key} {};
    explicit YamlKey(long key)
    : _value{key} {};

private:
    using Type = std::variant<long, std::string>;
    Type _value;
};

}  // namespace

class NodeImpl {
public:
    explicit NodeImpl(const Node* self, YamlKey path, YAML::Node yaml)
        : _map{constructMap(self, yaml)}, _seq{constructSeq(self, yaml)}, _yaml{yaml}, _path{path} {}

    const Node& get(long key) const {
        if (key < 0 || key >= _seq.size()) {
            // TODO: throw
        }
        return *_seq[key];
    }

    const Node& get(std::string key) const {
        // TODO: error-handling
        return *_map.at(key);
    }

    const YAML::Node yaml() const {
        return _yaml;
    }

    explicit operator bool() const {
        return bool(_yaml);
    }

private:
    ChildrenMap _map;
    ChildrenSeq _seq;
    YAML::Node _yaml;
    YamlKey _path;
    //    bool _isValid;  -- call isDefined instead

    static ChildrenMap constructMap(const Node* parent, YAML::Node node) {
        ChildrenMap out;
        if (!node.IsMap()) {
            return out;
        }

        for (const auto kvp: node) {
            const auto key = kvp.first.as<std::string>();
            out.emplace(kvp.first.as<std::string>(), std::make_unique<Node>(parent, key, kvp.second));
        }

        return out;
    }

    static ChildrenSeq constructSeq(const Node* parent, YAML::Node node) {
        ChildrenSeq out;
        if (!node.IsSequence()) {
            return out;
        }

        long index = 0;
        for (const auto child: node) {
            out.emplace_back(std::make_unique<Node>(parent, index, child));
            index++;
        }

        return out;
    }};

const YAML::Node Node::yaml() const {
    return _impl->yaml();
}

Node::operator bool() const {
    return _impl->operator bool();
}

Node::~Node() = default;

Node::Node(const Node* parent, std::string path, YAML::Node yaml)
    : _impl{std::make_unique<NodeImpl>(this, YamlKey{path}, yaml)} {}


Node::Node(const Node* parent, long path, YAML::Node yaml)
    : _impl{std::make_unique<NodeImpl>(this, YamlKey{path}, yaml)} {}

const Node& Node::operator[](long key) const {
    return _impl->get(key);
}
const Node& Node::operator[](std::string key) const {
    return _impl->get(key);
}

NodeSource::NodeSource(std::string yaml, std::string path)
: _yaml{YAML::Load(yaml)},
  _root{std::make_unique<Node>(nullptr, "", _yaml)},
  _path{path} {}

