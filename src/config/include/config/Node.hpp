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
 * Throw this to indicate a bad conversion.
 */
class InvalidConversionException : public std::exception {
 public:
    InvalidConversionException(const class Node* node,
                               const YAML::BadConversion& yamlException,
                               const std::type_info& destType);

    const char* what() const noexcept override {
        return _what.c_str();
    }

 private:
    std::string _what;
};

/**
 * Throw this to indicate bad input yaml syntax.
 */
class InvalidYAMLException : public std::exception {
public:
    InvalidYAMLException(const std::string& path, const YAML::ParserException& yamlException);

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    std::string _what;
};

/**
 * Throw this to indicate a bad path.
 */
class InvalidKeyException : public std::exception {
public:
    InvalidKeyException(const std::string& msg, const class Node* node);

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    std::string _what;
};

enum class NodeType {
    Undefined,
    Null,
    Scalar,
    Sequence,
    Map,
};

class Node {
public:
    ~Node();
    /**
     * @return
     *   what type we are.
     */
    NodeType type() const;
    /**
     * @return
     *   if we're a scalar type (string, number, etc).
     */
    bool isScalar() const;
    /**
     * @return
     *   if we're a sequence (array) type.
     */
    bool isSequence() const;
    /**
     * @return
     *   if we're a map type.
     */
    bool isMap() const;
    /**
     * @return
     *   if we're specified as `null`.
     *   This is not the same as not being defined.
     */
    bool isNull() const;
    size_t size() const;

    std::string path() const;
    explicit operator bool() const;

    template <typename K>
    Node operator[](K&& key) const {
        if constexpr (std::is_convertible_v<K, std::string>) {
            return stringGet(key);
        } else {
            static_assert(std::is_convertible_v<K, long>);
            return longGet(long(key));
        }
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
     *   Like `operation[]`, this will check its chain of parent nodes and inherit the value if it
     *   exists anywhere in the chain. If the value is not specified in any parent node, it will
     *   return an empty optional (`std::nullopt`).
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
                "3.  is a type built into YAML::Node e.g. int, string, vector<built-in-type>"
                "    etc.");
        if (!*this) {
            return std::nullopt;
        }
        try {
            return _maybeImpl<O, Args...>(std::forward<Args>(args)...);
        } catch (const YAML::BadConversion& x) {
            BOOST_THROW_EXCEPTION(InvalidConversionException(this, x, typeid(O)));
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
     *   Like `operation[]`, this will check its chain of parent nodes and inherit the value if it
     *   exists anywhere in the chain. If the value is not specified in any parent node, it will
     *   throw an `InvalidKeyException`. If you want to allow a value to be missing, use
     *   `.maybe<O>()`.
     * @throws InvalidKeyException
     *   if key not found
     * @throws InvalidConversionException
     *   if cannot convert to O or if value is not specified
     */
    template <typename O, typename... Args>
    O to(Args&&... args) const {
        auto out = maybe<O, Args...>(std::forward<Args>(args)...);
        if (!out) {
            BOOST_THROW_EXCEPTION(
                    InvalidKeyException("Tried to access node that doesn't exist.", this));
        }
        return *out;
    }

    /**
     * @param out
     *   output stream to dump `node` to in YAML format
     * @param node
     *   node to dump to `out`
     * @return out
     */
    friend std::ostream& operator<<(std::ostream& out, const Node& node) {
        return out << YAML::Dump(node.yaml());
    }


private:
    friend class NodeImpl;
    friend class NodeSource;
    friend class NodeSourceImpl;
    const std::unique_ptr<class NodeImpl> _impl;

    YAML::Node yaml() const;


    Node(std::string key, const class NodeImpl* parent);
    Node(long key, const class NodeImpl* parent);

    Node stringGet(std::string key) const;
    Node longGet(long key) const;

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
                    std::is_same_v<decltype(YAML::convert<O>::encode(std::declval<O>())),
                    YAML::Node>>,
            typename = void,
            typename = void>
    std::optional<O> _maybeImpl(Args&&... args) const {
        static_assert(sizeof...(args) == 0,
                      "Cannot pass additional args when using built-in YAML conversion");
        return std::make_optional<O>(yaml().as<O>());
    }
};

class NodeSource {
public:
    ~NodeSource();
    NodeSource(std::string yaml, std::string path);
    Node root() const;

private:
    const std::unique_ptr<class NodeSourceImpl> _impl;
};

}  // namespace genny

#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
