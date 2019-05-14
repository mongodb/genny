#include <variant>
#include <string>
#include <stack>
#include <vector>
#include <memory>

#include <catch2/catch.hpp>

#include <yaml-cpp/yaml.h>


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


class YamlPath {
public:
    template<typename K>
    YamlPath operator[](K k) const {
        return then(k);
    }

    template<typename K>
    YamlPath then(K next) const {
        YamlPath out = *this;
        out._keys.emplace_back(next);
        return out;
    }
    friend std::ostream& operator<<(std::ostream& out, const YamlPath& key) {
        key.appendTo(out);
        return out;
    }
    std::string toString() const {
        std::stringstream out;
        out << *this;
        return out.str();
    }

    YamlPath normalize() const {
        std::stack<YamlKey> components;
        for(const auto& key : _keys) {
            if (key.isParent()) {
                components.pop();
                continue;
            }
            components.push(key);
        }
        std::stack<YamlKey> reversed;
        while(!components.empty()) {
            reversed.push(components.top());
            components.pop();
        }

        YamlPath out;
        while(!reversed.empty()) {
            out = out.then(reversed.top());
            reversed.pop();
        }

        return out;
    }

    const YAML::Node apply(YAML::Node root) const {
        std::stack<YAML::Node> out;
        out.push(root);
        for(const auto& key : _keys) {
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
private:
    void appendTo(std::ostream& out) const {
        out << "/";
        for(size_t i = 0; i < _keys.size(); ++i) {
            out << _keys[i];
            if (i < _keys.size() - 1) {
                out << "/";
            }
        }
    }
    std::vector<YamlKey> _keys;
};


//class NodeSource {
//
//private:
//    YAML::Node _yaml;
//};
//
//class PathedNode {
//
//};


/////////////////////////////

TEST_CASE("Access path") {
    YamlPath p;

    REQUIRE(p.toString() == "/");

    {
        auto p2 = p.then("foo");
        REQUIRE(p2.toString() == "/foo");
    }

    {
        auto p2 = p.then(0);
        REQUIRE(p2.toString() == "/0");
    }

    REQUIRE(p["foo"][0]["bar"][".."].toString() == "/foo/0/bar/..");
    REQUIRE(p["foo"][0]["bar"][".."].normalize().toString() == "/foo/0");
};

TEST_CASE("apply") {
    YAML::Node foo = YAML::Load(R"(
foo: bar
nested: child
)");
    YamlPath p;
    REQUIRE((p["foo"][0][".."].apply(foo).as<std::string>() == "bar"));

    REQUIRE(p["foo"].apply(foo).as<std::string>() == "bar");
}

