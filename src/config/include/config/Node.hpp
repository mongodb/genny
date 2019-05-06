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
 * Specialize this type if you wish to provide
 * a conversion function for `O` but you can't
 * create a new constructor on `O` that takes in
 * a `const Node&` as its first parameter.
 */
template <typename O>
struct NodeConvert {};

/**
 * Throw this to indicate a bad path.
 */
class InvalidKeyException : public std::exception {
public:
    InvalidKeyException(const std::string& msg, const std::string& key, const class Node* node)
        : _what{createWhat(msg, key, node)} {}

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    static std::string createWhat(const std::string& msg,
                                  const std::string& key,
                                  const class Node* node);
    std::string _what;
};

/**
 * Throw this to indicate a bad conversion.
 */
class InvalidConversionException : public std::exception {
public:
    InvalidConversionException(const Node& node,
                               const YAML::BadConversion& yamlException,
                               const std::type_info& destType)
        : _what{createWhat(node, yamlException, destType)} {}

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    static std::string createWhat(const Node& node,
                                  const YAML::BadConversion& yamlException,
                                  const std::type_info& destType);
    std::string _what;
};

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


/**
 * Access YAML configuration
 *
 * Example usage:
 *
 * ```c++
 * // use [] to traverse
 * auto bar = node["foo"]["bar"][0];
 *
 * // treat as boolean to see if the value
 * // was specified in the yaml:
 * if (bar) {
 *   // use .to<T>() to convert
 *   std::cout << "bar = " << bar.to<int>();
 * }
 *
 * // or use value_or:
 * int w = node["w"].value_or(1);
 *
 * // or use .maybe:
 * optional<int> optW = node["w"].maybe<int>();
 *
 * // convert to built-in APIs like std::vector and std::map:
 * auto ns = node["ns"].to<std::vector<int>>();
 *
 * // iterate over a sequence
 * // e.g. given yaml "ns: [1,2,3]"
 * for(auto n : node["ns"]) { ... }
 *
 * // or iterate over a map
 * // e.g. given yaml "vals: {a: A, b: B}"
 * for(auto kvp : node["vals"]) {
 *     auto key = kvp.first.to<std::string>();
 *     auto val = kvp.second.to<std::string>();
 * }
 *
 * // Or support syntax-sugar for plural values:
 * std::vector<int> nums = node.getPlural<int>("num", "nums");
 * // this allows the user to specify either `num:7` or `nums:[1,2,3]`.
 * // See docs on `getPlural` for more info.
 * ```
 *
 * All values "inherit" from their parent nodes, so if you call
 * `node["foo"]["bar"].to<int>()` it will fall back to
 * `node["foo"].to<int>()` if `bar` isn't defined. If you wish
 * to explicitly access a parent value, use the key `..`.
 * So `node["foo"]["bar"][".."]` is roughly equivalent to `node["foo"]`.
 * ("Roughly" only because we still report the ".." as part of the path
 * in error-messages.)
 *
 * The API for `genny::Node` is strongly inspired by that of `YAML::Node`,
 * but it provides better error-reporting in the case of invalid
 * configuration and it allows conversion functions to pass in additional
 * arguments.
 *
 * To convert to non-primitive/built-in types you have two options:
 *
 * 1.  Add a constructor to your type that takes a `const Node&` as the
 *     first parameter. You can pass additional constructor-args in the
 *     call to `.to<T>()` or `.maybe<T>()`:
 *
 *     ```c++
 *     struct MyFoo {
 *       MyFoo(const Node& n, int x) {...};
 *     };
 *     MyFoo mf = node["foo"].to<MyFoo>(7);
 *     // calls MyFoo(node["foo"], 7)
 *     ```
 *
 * 2.  Specialize the `genny::NodeConvert` struct for the type:
 *
 *     ```c++
 *     namespace genny {
 *     template<>
 *     struct NodeConvert<MyFoo2> {
 *       // required due to SFINAE
 *       using type = MyFoo2;
 *       static type convert(const Node& node, int x) {
 *         ...
 *       };
 *     };
 *     }  // namespace genny
 *
 *     MyFoo2 mf2 = node["foo2"].to<MyFoo2>(7);
 *     // calls NodeConvert<MyFoo2>::convert(node["foo2"], 7);
 *     ```
 *
 * Whenever possible, prefer the first method of creating a constructor.
 * This allows you to keep all your logic for how to construct your type
 * with your type itself.
 *
 * Note that it is intentionally impossible to convert `genny::Node` into
 * `YAML::Node`.
 */
class Node {
public:
    /**
     * @param yaml
     *   source yaml
     * @param key
     *   key to associate for this node. This could be a file-name or a meaningful way
     *   of telling the user where the yaml came from. This is used primarily for error-reporting.
     */
    Node(const std::string& yaml, std::string key)
        : Node{parse(yaml, key), nullptr, std::move(key)} {}

    // explicitly allow copy and move
    // this is here to protect against regressions
    // accidentally making this non-copy/move-able

    /**
     * Copy-construct.
     */
    Node(const Node&) = default;

    /**
     * Move-construct.
     */
    Node(Node&&) = default;

    /**
     * @return a copy of this node.
     */
    Node& operator=(const Node&) = default;

    /**
     * Usage of this node in a moved-from state is undefined.
     *
     * @return a moved-to version of this node.
     */
    Node& operator=(Node&&) = default;

    /**
     * What type of node we are.
     */
    enum class NodeType {
        Undefined,
        Null,
        Scalar,
        Sequence,
        Map,
    };

    /**
     * Extract the value via `.to<T>()` if the node
     * is valid else return the fallback value.
     *
     * Deduction allows you to omit the `T` if it matches
     * the `T` fallback:
     *
     * ```c++
     * auto x = node.value_or(7); // int
     * auto y = node.value_or(std::string{"foo"}); // std::string
     *
     * // or specify it the hard way
     * auto z = node.value_or<std::string>("foo");
     * ```
     *
     * Like `operator[]` this will "fall-back" to the parent node.
     * So `node["foo"]["bar"].value_or(8)` will fall-back to
     * `node["foo"].value_or(8)` if `node["foo"]["bar"]` isn't
     * specified.
     *
     * @tparam T
     *   output type
     * @param fallback
     *   value to use if this is undefined
     * @return
     *   result of converting to `T` (via `::to<T>()`) or `fallback` if this node isn't valid
     *   or is null.
     */
    template <typename T>
    T value_or(T&& fallback) const {
        if (!(*this) || this->isNull()) {
            return fallback;
        }
        if (_yaml) {
            return to<T>();
        } else {
            return fallback;
        }
    }

    /**
     * @tparam O
     *   output type
     * @tparam Args
     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
     *   genny::Node& node, Args...) {...} };` impl.
     * @param args
     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
     *   genny::Node& node, Args...) {...} };` impl.
     * @return
     *   the result of converting this node to O either via its constructor or via the
     *   `NodeConvert<O>::convert` function.
     *   Like `operator[]` and other methods on `Node`, we "fallback" to the parent node
     *   if this node doesn't have a value specified.
     * @throws InvalidKeyException
     *   if key not found
     * @throws InvalidConversionException
     *   if cannot convert to O
     */
    template <typename O, typename... Args>
    O to(Args&&... args) const {
        auto out = maybe<O, Args...>(std::forward<Args>(args)...);
        if (!out) {
            BOOST_THROW_EXCEPTION(
                InvalidKeyException("Tried to access node that doesn't exist.", this->_key, this));
        }
        return *out;
    }

    /**
     * @tparam O
     *   output type. Cannot convert to `YAML::Node` (we enforce this at compile-time).
     * @tparam Args
     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
     *   genny::Node& node, Args...) {...} };` impl.
     * @param args
     *   any additional arguments to forward to the `O` constructor in addition to `*this` *or* to
     * pass to the `template<> class genny::NodeConvert<O> { using type=O; static O convert(const
     *   genny::Node& node, Args...) {...} };` impl.
     * @return
     *   A `nullopt` if this (or parent) node isn't defined.
     *   Else the result of converting this node to O either via its constructor or via the
     *   `NodeConvert<O>::convert` function.
     *   Like `operator[]` and other methods on `Node`, we "fallback" to the parent node
     *   if this node doesn't have a value specified.
     */
    template <typename O = Node, typename... Args>
    std::optional<O> maybe(Args&&... args) const {
        // Doesn't seem possible to test these static asserts since you'd get a compiler error
        // (by design) when trying to call node.to<YAML::Node>().
        static_assert(!std::is_same_v<std::decay_t<YAML::Node>, std::decay_t<O>>,
                      "Cannot convert to YAML::Node");
        static_assert(
            // This isn't the most reliable static_assert but hopefully this block
            // makes debugging compiler-errors easier.
            std::is_same_v<decltype(_maybeImpl<O, Args...>(std::forward<Args>(args)...)),
                           std::optional<O>>,
            "Destination type must satisfy at least *one* of the following:\n"
            "\n"
            "1.  is constructible from `const Node&` and the given arguments\n"
            "2.  has a `NodeConvert` struct like the following:\n"
            "\n"
            "        namespace genny {\n"
            "        template<> struct NodeConvert<Foo> {\n"
            "            using type = Foo;\n"
            "            static Foo convert(const Node& node, other, args) { ... }\n"
            "        };\n"
            "        }  // namesace genny\n"
            "\n"
            "3.  is a type built into YAML::Node e.g. int, string, vector<built-in-type> etc.");
        if (!*this) {
            return std::nullopt;
        }
        try {
            return _maybeImpl<O, Args...>(std::forward<Args>(args)...);
        } catch (const YAML::BadConversion& x) {
            BOOST_THROW_EXCEPTION(InvalidConversionException(*this, x, typeid(O)));
        }
    }

    /**
     * @tparam K
     *   key type (either a string for maps or an int for sequences)
     * @param key
     *   key to access. Can use the special key ".." to explicitly access a value from the parent
     *   node.
     * @return
     *   Node at the given key.
     *
     *   This does *not* throw if the key isn't present. If they key isn't present in the parent
     *   node it will try to find it in the parent node recursively. If it can't be found,
     *   the node will be 'invalid' (and return false to `operator bool()` and the calls to
     * `.maybe()` will return a `nullopt`. Calls to `.to<>()` will fail for invalid nodes.
     */
    template <typename K>
    const Node operator[](const K& key) const {
        return this->get(key);
    }

    /**
     * Extract a vector of items by supporting both singular and plural keys.
     *
     * Example YAML that this supports:
     *
     * ```yaml
     * # Calling getPlural<int>("Number","Numbers") returns [7]
     * Foo:
     *   Number: 7
     *
     * # Calling getPlural<int>("Number","Numbers") returns [1,2]
     * Bar:
     *   Numbers: [1,2]
     * ```
     *
     * The node cannot have both keys present. The following
     * will error:
     *
     * ```yaml
     * # Calling getPlural<int>("Bad","Bads")
     * # will throw because the node must have
     * # exactly one of the keys
     * BadExample:
     *   Bad: 7
     *   Bads: [1,2]
     * ```
     *
     * If the value at the plural key isn't a sequence we also barf:
     *
     * ```yaml
     * # Calling getPlural<int>("Bad","Bads") will fail
     * # because Bads isn't a sequence.
     * AnotherBadExample:
     *   Bads: 3
     * ```
     *
     * @tparam T
     *   the returned type. Must align with the return-type of `F`.
     * @tparam F
     *   type of the callback/mapping function that maps from `Node` to `T`.
     * @param singular
     *   the singular version of the key e.g. "Number"
     * @param plural
     *   the plural version of the key e.g. "Numbers"
     * @param f
     *   callback function mapping from the found node to a `T` instance. If not specified,
     *   will use `to<T>()`
     * @return
     *   a `vector<T>()` formed by applying `f` to each item in the sequence found at `this[plural]`
     *   or, if that is not defined, by applying `f` to the single-item sequence `[ this[singular]
     * ]`.
     */
    // The implementation is below because we require the full class definition
    // within the implementation.
    template <typename T, typename F = std::function<T(const Node&)>>
    std::vector<T> getPlural(
        const std::string& singular,
        const std::string& plural,
        // Default conversion function is `node.to<T>()`.
        F&& f = [](const Node& n) { return n.to<T>(); }) const;

    /**
     * @param out
     *   output stream to dump `node` to in YAML format
     * @param node
     *   node to dump to `out`
     * @return out
     */
    friend std::ostream& operator<<(std::ostream& out, const Node& node) {
        return out << YAML::Dump(node._yaml);
    }

    /**
     * @return
     *   number of child elements. This is the number of
     *   elements in a sequence or (k,v) pairs in a map. The size
     *   of scalar and null nodes is zero as is the size of
     *   undefined/non-existant nodes.
     */
    auto size() const {
        return _yaml.size();
    }

    /**
     * @return
     *   if this node **is defined**.
     *
     *   Note that this is not the same as `.to<bool>()`!
     *
     *   If you have YAML `foo: false`, the value of `bool(node["foo"])` is true.
     */
    explicit operator bool() const {
        return _valid && _yaml;
    }

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
        if (!*this) {
            return NodeType::Undefined;
        }
        auto yamlTyp = _yaml.Type();
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

    /**
     * @return
     *   the path that we took to get here. Path elements are
     *   separated by slashes.
     *
     * Given the following yaml:
     *
     * ```yaml
     * foo: [1, 2]
     * bar: baz
     * ```
     *
     * 1. The path to `1` is `/foo/0`.
     * 2. The path to `2` is `/foo/1`.
     * 3. The path to `baz` is `/foo/bar`.
     *
     * **Paths for Keys in Sequences and Maps**:
     *
     * When iterating over maps, the keys technically have their own paths
     * as well. For example:
     *
     * ```c++
     * Node node {
     *   "foo: [1, 2]\n"
     *   "bar: baz", ""
     * };
     *
     * for(auto kvp : node) {
     *   // First iteration:
     *   // - kvp.first is the 'foo' key and its path is `/foo$key`
     *   // - kvp.second is the `[1,2]` value and its path is `/foo`.
     *   //
     *   // Second iteration:
     *   // - kvp.first is the `bar` key and its path is `/bar$key`
     *   // - kvp.second is the `baz` value and its path is `/bar`.
     * }
     * ```
     *
     * This is more of a curiosity than a useful feature. It is used when giving
     * error-messages.
     */
    std::string path() const;

    /** used in iteration */
    struct iterator;

    /** start iterating */
    iterator begin() const;

    /** end iterator */
    iterator end() const;

    /** the value-type of the iterator. */
    // friends because we need to use private ctors
    friend class IteratorValue;

private:
    Node(const YAML::Node yaml, const Node* const parent, bool valid, std::string key)
        : _yaml{yaml}, _parent{parent}, _valid{valid}, _key{std::move(key)} {}

    Node(const YAML::Node yaml, const Node* const parent, std::string key)
        : Node{yaml, parent, yaml, std::move(key)} {}


    // helper type-function
    template <typename O, typename... Args>
    static constexpr bool isNodeConstructible() {
        // exclude is_trivially_constructible_v values because
        // for some reason `int` and other primitives report as is_constructible here for some
        // reason.
        return !std::is_trivially_constructible_v<O> &&
            std::is_constructible_v<O, const Node&, Args...>;
    }

    // Simple case where we have `O(const Node&,Args...)` constructor.
    template <typename O,
              typename... Args,
              typename = std::enable_if_t<isNodeConstructible<O, Args...>()>>
    std::optional<O> _maybeImpl(Args&&... args) const {
        // If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
        return std::make_optional<O>(*this, std::forward<Args>(args)...);
    }

    template <typename O,
              typename... Args,
              typename = std::enable_if_t<
                  // don't have an `O(const Node&, Args...)` constructor
                  !isNodeConstructible<O, Args...>() &&
                  // rely on sfinae to determine if we have a `NodeConvert<O>` definition.
                  std::is_same_v<O, typename NodeConvert<O>::type> &&
                  // a bit pedantic but also require that we can call
                  // `NodeConvert<O>::convert` and get back an O.
                  std::is_same_v<O,
                                 decltype(NodeConvert<O>::convert(std::declval<Node>(),
                                                                  std::declval<Args>()...))>>,
              typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        // If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
        return std::make_optional<O>(NodeConvert<O>::convert(*this, std::forward<Args>(args)...));
    }

    // This will get used if the ↑ fails to instantiate due to SFINAE.
    template <
        typename O,
        typename... Args,
        typename = std::enable_if_t<
            // don't have an `O(const Node&, Args...)` constructor
            !isNodeConstructible<O, Args...>() &&
            // and the `YAML::convert<O>` struct has been defined.
            std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())), YAML::Node>>,
        typename = void,
        typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        static_assert(sizeof...(args) == 0,
                      "Cannot pass additional args when using built-in YAML conversion");
        // If you get compiler errors in _maybeImpl, see the note on the public maybe() function.
        return std::make_optional<O>(_yaml.as<O>());
    }

    // Helper to parse yaml string and throw a useful error message if parsing fails
    static YAML::Node parse(std::string yaml, std::string path);

    // helper for yamlGet that returns nullopt if no parent
    template <typename K>
    std::optional<const YAML::Node> parentGet(const K& key) const {
        if (!_parent) {
            return std::nullopt;
        }
        return _parent->yamlGet(key);
    }

    // Forward to _yaml[key] if we're a valid node else forward to the parent
    template <typename K>
    std::optional<const YAML::Node> yamlGet(const K& key) const {
        if (!_valid) {
            return parentGet(key);
        }
        const YAML::Node found = _yaml[key];
        if (found) {
            return std::make_optional<const YAML::Node>(found);
        }
        return parentGet(key);
    }

    template <typename K>
    const Node get(const K& key) const {
        const std::string keyStr = v1::toString(key);
        if constexpr (std::is_convertible_v<K, std::string>) {
            if (key == "..") {
                if (!_parent) {
                    return Node{YAML::Node{}, nullptr, false, _key + "/.."};
                }

                std::ostringstream parentKey;
                parentKey << _parent->_key << "/" << _key << "/..";
                return Node{_parent->_yaml, _parent->_parent, _parent->_valid, parentKey.str()};
            }
        }
        try {
            auto yaml = this->yamlGet(key);
            return yaml ? Node{*yaml, this, true, keyStr} : Node{YAML::Node{}, this, false, keyStr};
        } catch (const YAML::Exception& x) {
            // YAML::Node is inconsitent about where it throws exceptions for `node[0]` versus
            // `node["foo"]`.
            BOOST_THROW_EXCEPTION(InvalidKeyException(
                "Invalid YAML access. Perhaps trying to treat a map as a sequence?",
                v1::toString(key),
                this));
        }
    }

    void buildPath(std::stringstream& out) const;

    YAML::Node _yaml;
    std::string _key;
    const Node* _parent;
    bool _valid;
};


// we can act like a Node if iterated value is a scalar or
// we can act like a pair of Nodes if iterated value is a map entry
class IteratorValue : public std::pair<Node, Node>, public Node {
public:
    // jump through immense hoops to avoid knowing anything about the actual yaml iterator other
    // than its pair form is a pair of {YAML::Node, YAML::Node}
    template <typename ITVal>
    IteratorValue(const Node* parent, ITVal itVal, size_t index)
        :  // API like kvp when itVal is being used in map context
          NodePair{std::make_pair(
              Node{itVal.first,
                   parent,
                   bool(itVal.first),  // itVal.first will be falsy if we're iterating over a
                                       // sequence.
                   //
                   // For map-iteration cases the path for the kvp may as well
                   // be the index. See the 'YAML::Node Equivalency' test-cases.
                   (itVal.first ? (itVal.first.template as<std::string>()) : v1::toString(index)) +
                       "$key"},
              Node{itVal.second,
                   parent,
                   bool(itVal.second),  // itVal.second will be falsy if we're iterating over a
                                        // sequence
                   //
                   // The key for the value in map-iteration cases is
                   // itVal.first. And it's the index on sequence-iteration
                   // cases.
                   itVal.first ? itVal.first.template as<std::string>() : v1::toString(index)})},
          // API like a value where it's being used in sequence context
          Node{itVal, parent, itVal, v1::toString(index)} {}

private:
    using NodePair = std::pair<Node, Node>;
};


// Wrap a YAML iterator and also keep track of
// the index on sequences for use in error-messaging.
class Node::iterator {
public:
    auto operator++() {
        ++index;
        return _child.operator++();
    }

    auto operator*() const {
        return IteratorValue{parent, (_child.operator*()), index};
    }

    auto operator-> () const {
        return IteratorValue{parent, (_child.operator*()), index};
    }

    auto operator==(const iterator& rhs) const {
        return _child == rhs._child;
    }

    auto operator!=(const iterator& rhs) const {
        return _child != rhs._child;
    }

private:
    // Don't expose or assume the type of YAML::Node.begin()
    using IterType = decltype(std::declval<const YAML::Node>().begin());

    iterator(IterType child, const Node* parent) : _child(std::move(child)), parent(parent) {}

    friend Node;

    IterType _child;
    const Node* parent;
    size_t index = 0;
};

// Need to define this out-of-line because we need the
// full class definition in order to iterate over
// all the nodes in the plural case.
template <typename T, typename F>
std::vector<T> Node::getPlural(const std::string& singular,
                               const std::string& plural,
                               F&& f) const {
    std::vector<T> out;
    auto pluralValue = (*this)[plural];
    auto singValue = (*this)[singular];
    if (pluralValue && singValue) {
        BOOST_THROW_EXCEPTION(
            // the `$plural(singular,plural)` key is kinda cheeky but hopefully
            // it helps to explain what the code tried to do in error-messages.
            InvalidKeyException("Can't have both '" + singular + "' and '" + plural + "'.",
                                "$plural(" + singular + "," + plural + ")",
                                this));
    } else if (pluralValue) {
        if (!pluralValue.isSequence()) {
            BOOST_THROW_EXCEPTION(
                InvalidKeyException("Plural '" + plural + "' must be a sequence type.",
                                    "$plural(" + singular + "," + plural + ")",
                                    this));
        }
        for (auto&& val : pluralValue) {
            T created = std::invoke(f, val);
            out.emplace_back(std::move(created));
        }
    } else if (singValue) {
        T created = std::invoke(f, singValue);
        out.emplace_back(std::move(created));
    } else if (!singValue && !pluralValue) {
        BOOST_THROW_EXCEPTION(
            InvalidKeyException("Either '" + singular + "' or '" + plural + "' required.",
                                "$plural(" + singular + "," + plural + ")",
                                this));
    }

    return out;
}

}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
