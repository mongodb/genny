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

#ifndef HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
#define HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/throw_exception.hpp>
#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

namespace v1 {
// Helper to convert arbitrary types to strings if they have operator<<(ostream&) defined.
// This is used to build up paths when calling `node["foo"]` and `node[0]` etc.
template <typename T>
std::string toString(const T& t) {
    std::stringstream out;
    out << t;
    return out.str();
}
}  // namespace v1

/**
 * Throw this to indicate bad input yaml syntax.
 */
class InvalidYAMLException : public std::exception {
public:
    InvalidYAMLException(const std::string& path, const YAML::ParserException& yamlException)
        : _what{createWhat(path, yamlException)} {}

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    static std::string createWhat(const std::string& node,
                                  const YAML::ParserException& yamlException);
    std::string _what;
};

enum class NodeType {
    Undefined,
    Null,
    Scalar,
    Sequence,
    Map,
};

class NodeImpl;

////////////////////////////////
// Node

// allowed to be stack-allocated
// may not have pointers to objects other than NodeImpls
class Node {
public:


    NodeType type() const;
    bool isScalar() const;
    bool isSequence() const;
    bool isMap() const;
    bool isNull() const;

    operator bool() const;

    template<typename K>
    Node operator[](K&& key) const {
        if constexpr (std::is_convertible_v<K,std::string>) {
            return stringGet(key);
        } else {
            static_assert(std::is_convertible_v<K,long>);
            return longGet(long(key));
        }
    }

private:
    friend class NodeImpl;
    friend class NodeSource;

    Node(const NodeImpl* impl, std::string  path)
    : _impl{impl}, _path{path} {}
    const NodeImpl* _impl;
    const std::string _path;

    Node stringGet(const std::string& key) const;
    Node longGet(long key) const;
};


// Always owned by NodeSource (below)
class NodeImpl {
public:
    using Child = std::unique_ptr<NodeImpl>;
    using ChildSequence = std::vector<Child>;
    using ChildMap = std::map<std::string, Child>;

    NodeImpl(YAML::Node node, const NodeImpl* parent)
    : _node{node},
      _parent{parent},
      _nodeType{determineType(_node)},
      _childSequence(childSequence(node, this)),
      _childMap(childMap(node, this)) {}

    /**
     * @return
     *   if we're specified as `null`.
     *   This is not the same as not being defined.
     */
    bool isNull() const {
        return type() == NodeType::Null;
    }

    /**
     * @return
     *   if we're a scalar type (string, number, etc).
     */
    bool isScalar() const {
        return type() == NodeType::Scalar;
    }

    /**
     * @return
     *   if we're a sequence (array) type.
     */
    bool isSequence() const {
        return type() == NodeType::Sequence;
    }

    /**
     * @return
     *   if we're a map type.
     */
    bool isMap() const {
        return type() == NodeType::Map;
    }

    /**
     * @return
     *   what type we are.
     */
     NodeType type() const {
        return _nodeType;
     }

    template<typename K>
    const NodeImpl& get(K&& key) const {
        if constexpr (std::is_convertible_v<K, std::string>) {
            if(_nodeType == NodeType::Map) {
                return childMapGet(std::forward<K>(key));
            } else {
                // TODO: better error-messaging
                BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
            }
        } else {
            static_assert(std::is_constructible_v<K, size_t>);
            if (_nodeType == NodeType::Sequence) {
                return childSequenceGet(std::forward<K>(key));
            } else {
                // TODO: better error-messaging
                BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
            }
        }
    }

    const NodeImpl* stringGet(const std::string &key) const;
    const NodeImpl* longGet(long key) const;

private:
    const YAML::Node _node;
    const NodeImpl* _parent;
    const NodeType _nodeType;

    const ChildSequence _childSequence;
    const ChildMap _childMap;

    template<typename K>
    const NodeImpl& childMapGet(K&& key) const {
        assert(_nodeType == NodeType::Map);
        if (auto found = _childMap.find(key); found != _childMap.end()) {
            return *(found->second);
        }
        // TOOD
        BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
    }

    template<typename K>
    const NodeImpl& childSequenceGet(K&& key) const {
        assert(_nodeType == NodeType::Sequence);
        return *(_childSequence.at(key));
        // TODO: handle std::out_of_range
//        BOOST_THROW_EXCEPTION(std::invalid_argument("TODO"));
    }

    static ChildSequence childSequence(YAML::Node node, const NodeImpl* parent) {
        ChildSequence out;
        if (node.Type() != YAML::NodeType::Sequence) {
            return out;
        }
        for(const auto& kvp : node) {
            out.emplace_back(std::make_unique<NodeImpl>(kvp, parent));
        }
        return out;
    }

    static ChildMap childMap(YAML::Node node, const NodeImpl* parent) {
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

    static NodeType determineType(YAML::Node node) {
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
};


//////////////////////////////////
// NodeSource

// the owner of the root yaml node
class NodeSource {
public:
    NodeSource(std::string yaml, std::string path)
    : _root{parse(yaml, path), nullptr}, _path{std::move(path)} {}

    Node root() const {
        return {&_root, _path};
    }

private:
    const NodeImpl _root;
    std::string _path;

    // Helper to parse yaml string and throw a useful error message if parsing fails
    static YAML::Node parse(std::string yaml, const std::string& path);
};

}  // namespace genny



//namespace genny {
//
///**
// * Specialize this type if you wish to provide
// * a conversion function for `O` but you can't
// * create a new constructor on `O` that takes in
// * a `const Node&` as its first parameter.
// */
//template <typename O>
//struct NodeConvert {};
//
///**
// * Throw this to indicate a bad path.
// */
//class InvalidKeyException : public std::exception {
//public:
//    InvalidKeyException(const std::string& msg, const std::string& key, const class Node* node)
//        : _what{createWhat(msg, key, node)} {}
//
//    const char* what() const noexcept override {
//        return _what.c_str();
//    }
//
//private:
//    static std::string createWhat(const std::string& msg,
//                                  const std::string& key,
//                                  const class Node* node);
//    std::string _what;
//};
//
///**
// * Throw this to indicate a bad conversion.
// */
//class InvalidConversionException : public std::exception {
//public:
//    InvalidConversionException(const Node& node,
//                               const YAML::BadConversion& yamlException,
//                               const std::type_info& destType)
//        : _what{createWhat(node, yamlException, destType)} {}
//
//    const char* what() const noexcept override {
//        return _what.c_str();
//    }
//
//private:
//    static std::string createWhat(const Node& node,
//                                  const YAML::BadConversion& yamlException,
//                                  const std::type_info& destType);
//    std::string _what;
//};
//
//
//

//
///**
// * Access YAML configuration
// *
// * Example usage:
// *
// * ```c++
// * // use [] to traverse
// * auto bar = node["foo"]["bar"][0];
// *
// * // treat as boolean to see if the value
// * // was specified in the yaml:
// * if (bar) {
// *   // use .to<T>() to convert
// *   std::cout << "bar = " << bar.to<int>();
// * }
// *
// * // or use .maybe<int>().value_or:
// * int w = node["w"].maybe<int>().value_or(1);
// *
// * // convert to built-in APIs like std::vector and std::map:
// * auto ns = node["ns"].to<std::vector<int>>();
// *
// * // iterate over a sequence
// * // e.g. given yaml "ns: [1,2,3]"
// * for(auto n : node["ns"]) { ... }
// *
// * // or iterate over a map
// * // e.g. given yaml "vals: {a: A, b: B}"
// * for(auto kvp : node["vals"]) {
// *     auto key = kvp.first.to<std::string>();
// *     auto val = kvp.second.to<std::string>();
// * }
// *
// * // Or support syntax-sugar for plural values:
// * std::vector<int> nums = node.getPlural<int>("num", "nums");
// * // this allows the user to specify either `num:7` or `nums:[1,2,3]`.
// * // See docs on `getPlural` for more info.
// * ```
// *
// * All values "inherit" from their parent nodes, so if you call `node["foo"]["bar"].to<int>()` the
// * 'bar' node will check its chain of parent nodes and inherit the value if it exists anywhere in
// * the chain. If you wish to explicitly access a parent value, use the key `..`. So
// * `node["foo"]["bar"][".."]` is roughly equivalent to `node["foo"]`.
// * ("Roughly" only because we still report the ".." as part of the path
// * in error-messages.)
// *
// * The API for `genny::Node` is strongly inspired by that of `YAML::Node`,
// * but it provides better error-reporting in the case of invalid
// * configuration and it allows conversion functions to pass in additional
// * arguments.
// *
// * To convert to non-primitive/built-in types you have two options:
// *
// * 1.  Add a constructor to your type that takes a `const Node&` as the
// *     first parameter. You can pass additional constructor-args in the
// *     call to `.to<T>()` or `.maybe<T>()`:
// *
// *     ```c++
// *     struct MyFoo {
// *       MyFoo(const Node& n, int x) {...};
// *     };
// *     MyFoo mf = node["foo"].to<MyFoo>(7);
// *     // calls MyFoo(node["foo"], 7)
// *     ```
// *
// * 2.  Specialize the `genny::NodeConvert` struct for the type:
// *
// *     ```c++
// *     namespace genny {
// *     template<>
// *     struct NodeConvert<MyFoo2> {
// *       // required due to SFINAE
// *       using type = MyFoo2;
// *       static type convert(const Node& node, int x) {
// *         ...
// *       };
// *     };
// *     }  // namespace genny
// *
// *     MyFoo2 mf2 = node["foo2"].to<MyFoo2>(7);
// *     // calls NodeConvert<MyFoo2>::convert(node["foo2"], 7);
// *     ```
// *
// * Whenever possible, prefer the first method of creating a constructor.
// * This allows you to keep all your logic for how to construct your type
// * with your type itself.
// *
// * Note that it is intentionally impossible to convert `genny::Node` into
// * `YAML::Node`.
// */
//class Node {
//public:
//    /**
//     * @param yaml
//     *   source yaml
//     * @param key
//     *   key to associate for this node. This could be a file-name or a meaningful way
//     *   of telling the user where the yaml came from. This is used primarily for error-reporting.
//     */
//    Node(const std::string& yaml, const std::string& path)
//        : Node{parse(yaml, path), nullptr} {}
//
//    Node(const Node&) = delete;
//    Node(Node&&) = delete;
//    void operator=(const Node&) = delete;
//    void operator=(Node&&) = delete;
//
//    /**
//     * @tparam K
//     *   key type (either a string for maps or an int for sequences)
//     * @param key
//     *   key to access. Can use the special key ".." to explicitly access a value from the parent
//     *   node.
//     * @return
//     *   Node at the given key.
//     *
//     *   This does *not* throw if the key isn't present. If they key isn't present in the parent
//     *   node it will try to find it in the parent node recursively. If it can't be found,
//     *   the node will be 'invalid' (and return false to `operator bool()` and the calls to
//     * `.maybe()` will return a `nullopt`. Calls to `.to<>()` will fail for invalid nodes.
//     */
//    template <typename K>
//    const Node operator[](const K& key) const {
//        return this->get(key);
//    }
//
//    /**
//     * @param out
//     *   output stream to dump `node` to in YAML format
//     * @param node
//     *   node to dump to `out`
//     * @return out
//     */
//    friend std::ostream& operator<<(std::ostream& out, const Node& node) {
//        return out << YAML::Dump(node._yaml);
//    }
//
//    /**
//     * @return
//     *   number of child elements. This is the number of
//     *   elements in a sequence or (k,v) pairs in a map. The size
//     *   of scalar and null nodes is zero as is the size of
//     *   undefined/non-existant nodes.
//     */
//    auto size() const {
//        return _yaml.size();
//    }
//
//    /**
//     * @return
//     *   if this node **is defined**.
//     *
//     *   Note that this is not the same as `.to<bool>()`!
//     *
//     *   If you have YAML `foo: false`, the value of `bool(node["foo"])` is true.
//     */
//    explicit operator bool() const {
//        return _valid && _yaml;
//    }
//
//
//    /**
//     * @return
//     *   the path that we took to get here. Path elements are
//     *   separated by slashes.
//     *
//     * Given the following yaml:
//     *
//     * ```yaml
//     * foo: [1, 2]
//     * bar: baz
//     * ```
//     *
//     * 1. The path to `1` is `/foo/0`.
//     * 2. The path to `2` is `/foo/1`.
//     * 3. The path to `baz` is `/foo/bar`.
//     *
//     * **Paths for Keys in Sequences and Maps**:
//     *
//     * When iterating over maps, the keys technically have their own paths
//     * as well. For example:
//     *
//     * ```c++
//     * Node node {
//     *   "foo: [1, 2]\n"
//     *   "bar: baz", ""
//     * };
//     *
//     * for(auto kvp : node) {
//     *   // First iteration:
//     *   // - kvp.first is the 'foo' key and its path is `/foo$key`
//     *   // - kvp.second is the `[1,2]` value and its path is `/foo`.
//     *   //
//     *   // Second iteration:
//     *   // - kvp.first is the `bar` key and its path is `/bar$key`
//     *   // - kvp.second is the `baz` value and its path is `/bar`.
//     * }
//     * ```
//     *
//     * This is more of a curiosity than a useful feature. It is used when giving
//     * error-messages.
//     */
//    std::string path() const;
//
//    /** used in iteration */
//    struct iterator;
//
//    /** start iterating */
//    iterator begin() const;
//
//    /** end iterator */
//    iterator end() const;
//
//    /** the value-type of the iterator. */
//    // friends because we need to use private ctors
//    friend class IteratorValue;
//    friend class RNode;
//
//private:
//    const YAML::Node _yaml;
//
//    using Child = std::unique_ptr<Node>;
//    using ChildSequence = std::vector<Child>;
//    using ChildMap = std::map<std::string, Child>;
//
//    const Node* _parent;
//
//    const NodeType _type;
//
//    const ChildSequence _sequenceChildren;
//    const ChildMap _mapChildren;
//
//    bool _valid;
//
//    Node(const YAML::Node yaml, const Node* const parent, bool valid)
//        : _yaml{yaml}, _parent{parent}, _valid{valid}, _type{determineType(yaml)},
//          _mapChildren{constructMapChildren(yaml)}, _sequenceChildren(constructSequenceChildren(yaml)) {}
//
//    Node(const YAML::Node yaml, const Node* const parent)
//        : Node{yaml, parent, yaml} {}
//
//    static ChildSequence constructSequenceChildren(YAML::Node node) {
//        ChildSequence out;
//        return out;
//    }
//    static ChildMap constructMapChildren(YAML::Node node) {
//        ChildMap out;
//        return out;
//    }
//
//
//    // helper for yamlGet that returns nullopt if no parent
//    template <typename K>
//    const Node* parentGet(const K& key) const {
//        if (!_parent) {
//            return nullptr;
//        }
//        return _parent->yamlGet(key);
//    }
//
//    // Forward to _yaml[key] if we're a valid node else forward to the parent
//    template <typename K>
//    const Node* yamlGet(const K& key) const {
//        if (!_valid) {
//            return parentGet(key);
//        }
//        const YAML::Node found = _yaml[key];
//        if (found) {
//            return this;
//        }
//        return parentGet(key);
//    }
//
//    template <typename K>
//    const Node* get(const K& key) const {
//        const std::string keyStr = v1::toString(key);
//        if constexpr (std::is_convertible_v<K, std::string>) {
//            if (key == "..") {
//                return _parent;
//            }
//        }
//        try {
//            return this->yamlGet(key);
//        } catch (const YAML::Exception& x) {
//            // YAML::Node is inconsistent about where it throws exceptions for `node[0]` versus
//            // `node["foo"]`.
//            BOOST_THROW_EXCEPTION(InvalidKeyException(
//                "Invalid YAML access. Perhaps trying to treat a map as a sequence?",
//                v1::toString(key),
//                this));
//        }
//    }
//};
//
//
//// "Real" Node
//// TODO: rename this to Node and existing Node to NodeImpl
//struct RNode {
//    const Node* node;
//    std::string path;
//
//    template <typename O, typename... Args>
//    static constexpr bool isNodeConstructible() {
//        // exclude is_trivially_constructible_v values because
//        // for some reason `int` and other primitives report as is_constructible here for some
//        // reason.
//        return !std::is_trivially_constructible_v<O> &&
//               std::is_constructible_v<O, const Node&, Args...>;
//    }
//
//    operator bool() const {
//        return node && bool(node);
//    }
//
//    /**
//     * @tparam O
//     *   output type. Cannot convert to `YAML::Node` (we enforce this at compile-time).
//     * @tparam Args
//     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
//     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
//     *   genny::Node& node, Args...) {...} };` impl.
//     * @param args
//     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
//     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
//     *   genny::Node& node, Args...) {...} };` impl.
//     * @return
//     *   A `nullopt` if this (or parent) node isn't defined.
//     *   Else the result of converting this node to O either via its constructor or via the
//     *   `NodeConvert<O>::convert` function.
//     *   Like `operation[]`, this will check its chain of parent nodes and inherit the value if it
//     *   exists anywhere in the chain. If the value is not specified in any parent node, it will
//     *   return an empty optional (`std::nullopt`).
//     */
//    template <typename O = Node, typename... Args>
//    std::optional<O> maybe(Args&&... args) const {
//        // Doesn't seem possible to test these static asserts since you'd get a compiler error
//        // (by design) when trying to call node.to<YAML::Node>().
//        static_assert(!std::is_same_v<std::decay_t<YAML::Node>, std::decay_t<O>>,
//                      "Cannot convert to YAML::Node");
//        static_assert(
//                // This isn't the most reliable static_assert but hopefully this block
//                // makes debugging compiler-errors easier.
//                std::is_same_v<decltype(_maybeImpl<O, Args...>(std::forward<Args>(args)...)),
//                        std::optional<O>>,
//                "Destination type must satisfy at least *one* of the following:\n"
//                "\n"
//                "1.  is constructible from `const Node&` and the given arguments\n"
//                "2.  has a `NodeConvert` struct like the following:\n"
//                "\n"
//                "        namespace genny {\n"
//                "        template<> struct NodeConvert<Foo> {\n"
//                "            using type = Foo;\n"
//                "            static Foo convert(const Node& node, other, args) { ... }\n"
//                "        };\n"
//                "        }  // namesace genny\n"
//                "\n"
//                "3.  is a type built into YAML::Node e.g. int, string, vector<built-in-type> etc.");
//        if (!*this) {
//            return std::nullopt;
//        }
//        try {
//            return _maybeImpl<O, Args...>(std::forward<Args>(args)...);
//        } catch (const YAML::BadConversion& x) {
//            BOOST_THROW_EXCEPTION(InvalidConversionException(*this->node, x, typeid(O)));
//        }
//    }
//
//    /**
//     * @tparam O
//     *   output type
//     * @tparam Args
//     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
//     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
//     *   genny::Node& node, Args...) {...} };` impl.
//     * @param args
//     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
//     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
//     *   genny::Node& node, Args...) {...} };` impl.
//     * @return
//     *   the result of converting this node to O either via its constructor or via the
//     *   `NodeConvert<O>::convert` function.
//     *   Like `operation[]`, this will check its chain of parent nodes and inherit the value if it
//     *   exists anywhere in the chain. If the value is not specified in any parent node, it will
//     *   throw an `InvalidKeyException`. If you want to allow a value to be missing, use
//     *   `.maybe<O>()`.
//     * @throws InvalidKeyException
//     *   if key not found
//     * @throws InvalidConversionException
//     *   if cannot convert to O or if value is not specified
//     */
//    template <typename O, typename... Args>
//    O to(std::string path, Args&&... args) const {
//        auto out = maybe<O, Args...>(std::forward<Args>(args)...);
//        if (!out) {
//            BOOST_THROW_EXCEPTION(
//                    InvalidKeyException("Tried to access node that doesn't exist.", this->path, this->node));
//        }
//        return *out;
//    }
//
//    // Simple case where we have `O(const Node&,Args...)` constructor.
//    template <typename O,
//            typename... Args,
//            typename = std::enable_if_t<isNodeConstructible<O, Args...>()>>
//    std::optional<O> _maybeImpl(Args&&... args) const {
//        // If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
//        return std::make_optional<O>(*this, std::forward<Args>(args)...);
//    }
//
//    template <typename O,
//            typename... Args,
//            typename = std::enable_if_t<
//                    // don't have an `O(const Node&, Args...)` constructor
//                    !isNodeConstructible<O, Args...>() &&
//                    // rely on sfinae to determine if we have a `NodeConvert<O>` definition.
//                    std::is_same_v<O, typename NodeConvert<O>::type> &&
//                    // a bit pedantic but also require that we can call
//                    // `NodeConvert<O>::convert` and get back an O.
//                    std::is_same_v<O,
//                            decltype(NodeConvert<O>::convert(std::declval<Node>(),
//                                                             std::declval<Args>()...))>>,
//            typename = void>
//    std::optional<O> _maybeImpl(Args&&... args) const {
//        // If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
//        return std::make_optional<O>(NodeConvert<O>::convert(*this, std::forward<Args>(args)...));
//    }
//
//    // This will get used if the ↑ fails to instantiate due to SFINAE.
//    template <
//            typename O,
//            typename... Args,
//            typename = std::enable_if_t<
//                    // don't have an `O(const Node&, Args...)` constructor
//                    !isNodeConstructible<O, Args...>() &&
//                    // and the `YAML::convert<O>` struct has been defined.
//                    std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())), YAML::Node>>,
//            typename = void,
//            typename = void>
//    std::optional<O> _maybeImpl(Args&&... args) const;
//
//    /**
//     * Extract a vector of items by supporting both singular and plural keys.
//     *
//     * Example YAML that this supports:
//     *
//     * ```yaml
//     * # Calling getPlural<int>("Number","Numbers") returns [7]
//     * Foo:
//     *   Number: 7
//     *
//     * # Calling getPlural<int>("Number","Numbers") returns [1,2]
//     * Bar:
//     *   Numbers: [1,2]
//     * ```
//     *
//     * The node cannot have both keys present. The following
//     * will error:
//     *
//     * ```yaml
//     * # Calling getPlural<int>("Bad","Bads")
//     * # will throw because the node must have
//     * # exactly one of the keys
//     * BadExample:
//     *   Bad: 7
//     *   Bads: [1,2]
//     * ```
//     *
//     * If the value at the plural key isn't a sequence we also barf:
//     *
//     * ```yaml
//     * # Calling getPlural<int>("Bad","Bads") will fail
//     * # because Bads isn't a sequence.
//     * AnotherBadExample:
//     *   Bads: 3
//     * ```
//     *
//     * @tparam T
//     *   the returned type. Must align with the return-type of `F`.
//     * @tparam F
//     *   type of the callback/mapping function that maps from `Node` to `T`.
//     * @param singular
//     *   the singular version of the key e.g. "Number"
//     * @param plural
//     *   the plural version of the key e.g. "Numbers"
//     * @param f
//     *   callback function mapping from the found node to a `T` instance. If not specified,
//     *   will use `to<T>()`
//     * @return
//     *   a `vector<T>()` formed by applying `f` to each item in the sequence found at `this[plural]`
//     *   or, if that is not defined, by applying `f` to the single-item sequence `[ this[singular]
//     * ]`.
//     */
//    // The implementation is below because we require the full class definition
//    // within the implementation.
//    template <typename T, typename F = std::function<T(const Node&)>>
//    std::vector<T> getPlural(
//            const std::string& singular,
//            const std::string& plural,
//            // Default conversion function is `node.to<T>()`.
//            F&& f = [](const RNode& n) { return n.to<T>(); }) const;
//
//    // helper type-function
//    template<typename K>
//    std::pair<const RNode, const RNode> asIterPair(K k, size_t i) const {
//        // TODO
//        return {*this, *this};
//    }
//};
//
//
//
//// we can act like a Node if iterated value is a scalar or
//// we can act like a pair of Nodes if iterated value is a map entry
//class IteratorValue : public std::pair<const RNode, const RNode>, public RNode {
//public:
//    template <typename K>
//    IteratorValue(const RNode parent, K itVal, size_t index)
//        :  // API like kvp when itVal is being used in map context
//           RNodePair{parent.asIterPair(itVal, index)},
//           // API like a value where it's being used in sequence context
//           RNode{parent} {}
//
//private:
//    using RNodePair = std::pair<const RNode, const RNode>;
//};
//
//
//// Wrap a YAML iterator and also keep track of
//// the index on sequences for use in error-messaging.
//class Node::iterator {
//public:
//    auto operator++() {
//        ++index;
//        return _child.operator++();
//    }
//
//    auto operator*() const {
//        return IteratorValue{parent, (_child.operator*()), index};
//    }
//
//    auto operator-> () const {
//        return IteratorValue{parent, (_child.operator*()), index};
//    }
//
//    auto operator==(const iterator& rhs) const {
//        return _child == rhs._child;
//    }
//
//    auto operator!=(const iterator& rhs) const {
//        return _child != rhs._child;
//    }
//
//private:
//    // Don't expose or assume the type of YAML::Node.begin()
//    using IterType = decltype(std::declval<const YAML::Node>().begin());
//
//    iterator(IterType child, const Node* parent) : _child(std::move(child)), parent(parent) {}
//
//    friend Node;
//
//    IterType _child;
//    const Node* parent;
//    size_t index = 0;
//};
//
//template <
//        typename O,
//        typename... Args,
//        typename = std::enable_if_t<
//                // don't have an `O(const Node&, Args...)` constructor
//                !RNode::isNodeConstructible<O, Args...>() &&
//                // and the `YAML::convert<O>` struct has been defined.
//                std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())), YAML::Node>>,
//        typename = void,
//        typename = void>
//std::optional<O> RNode::_maybeImpl(Args&&... args) const {
//static_assert(sizeof...(args) == 0,
//              "Cannot pass additional args when using built-in YAML conversion");
//// If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
//return std::make_optional<O>(node->_yaml.as<O>());
//}
//
//
//// Need to define this out-of-line because we need the
//// full class definition in order to iterate over
//// all the nodes in the plural case.
//template <typename T, typename F>
//std::vector<T> Node::getPlural(const std::string& singular,
//                               const std::string& plural,
//                               F&& f) const {
//    std::vector<T> out;
//    auto pluralValue = (*this)[plural];
//    auto singValue = (*this)[singular];
//    if (pluralValue && singValue) {
//        BOOST_THROW_EXCEPTION(
//            // the `$plural(singular,plural)` key is kinda cheeky but hopefully
//            // it helps to explain what the code tried to do in error-messages.
//            InvalidKeyException("Can't have both '" + singular + "' and '" + plural + "'.",
//                                "$plural(" + singular + "," + plural + ")",
//                                this));
//    } else if (pluralValue) {
//        if (!pluralValue.isSequence()) {
//            BOOST_THROW_EXCEPTION(
//                InvalidKeyException("Plural '" + plural + "' must be a sequence type.",
//                                    "$plural(" + singular + "," + plural + ")",
//                                    this));
//        }
//        for (auto&& val : pluralValue) {
//            T created = std::invoke(f, val);
//            out.emplace_back(std::move(created));
//        }
//    } else if (singValue) {
//        T created = std::invoke(f, singValue);
//        out.emplace_back(std::move(created));
//    } else if (!singValue && !pluralValue) {
//        BOOST_THROW_EXCEPTION(
//            InvalidKeyException("Either '" + singular + "' or '" + plural + "' required.",
//                                "$plural(" + singular + "," + plural + ")",
//                                this));
//    }
//
//    return out;
//}
//
//}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
