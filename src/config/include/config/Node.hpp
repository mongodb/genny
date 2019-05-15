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
#include <variant>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <yaml-cpp/yaml.h>

template<typename T>
struct NodeConvert{};

class YamlKey {
public:
    explicit YamlKey(std::string key)
            : _value{key} {};
    explicit YamlKey(long key)
            : _value{key} {};

    bool operator<(const YamlKey& rhs) const {
        return _value < rhs._value;
    }

    std::string toString() const;

    friend std::ostream& operator<<(std::ostream& out, const YamlKey& key);

private:
    using Type = std::variant<long, std::string>;
    const Type _value;
};


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
    InvalidKeyException(const std::string& msg, const std::string& key, const class Node* node);

    const char* what() const noexcept override {
        return _what.c_str();
    }

private:
    std::string _what;
};

using KeyPath = std::vector<YamlKey>;

enum class NodeType {
    Undefined,
    Null,
    Scalar,
    Sequence,
    Map,
};

class Node {
public:
    Node(const Node&) = delete;
    void operator=(const Node&) = delete;
    Node(Node&&) = delete;
    void operator=(Node&&) = delete;

    const Node& operator[](long key) const;
    const Node& operator[](std::string key) const;
    ~Node();

    explicit operator bool() const;

    NodeType type() const;

    bool isScalar() const;


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
                    InvalidKeyException("Tried to access node that doesn't exist.", this->key(), this));
        }
        return *out;
    }

    bool isNull() const;
    bool isMap() const;
    bool isSequence() const;

    size_t size() const;

    std::string key() const;

    std::string path() const;

    class iterator begin() const;
    class iterator end() const;


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
    std::vector<T> getPlural(const std::string& singular,
                                   const std::string& plural,
                                    // Default conversion function is `node.to<T>()`.
                             F&& f = [](const Node& n) { return n.to<T>(); }) const;

    friend std::ostream& operator<<(std::ostream& out, const Node& node);

private:
    friend class iterator;
    friend class NodeImpl;
    friend class NodeSource;
    std::unique_ptr<class NodeImpl> _impl;
public:
    explicit Node(KeyPath path, YAML::Node yaml);
private:
    const YAML::Node yaml() const;

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

class iterator_value : public std::pair<const YamlKey,const Node&> {
public:
    iterator_value(YamlKey key, const Node& node);
private:
    friend Node;
};

class iterator {
public:
    ~iterator();
    bool operator!=(const iterator&) const;
    bool operator==(const iterator&) const;
    void operator++();
    const iterator_value operator*() const;
private:
    iterator(const class NodeImpl*, bool end);
    friend Node;
    friend class IteratorImpl;
    std::unique_ptr<class IteratorImpl> _impl;
};

class NodeSource {
public:
    ~NodeSource();
    const Node& root() const &{
        return *_root;
    }

    // disallow `NodeSource{"",""}.root();`
    void root() const && = delete;

    NodeSource(std::string yaml, std::string path);
private:
    YAML::Node _yaml;
    std::unique_ptr<Node> _root;
    std::string _path;
};

// A bulk of this could probably be moved to the PiMPL version
// in Node.cpp. Would just need `vector<const Node&> Node::getPlural(s,p)`
// and then map f over the result.
template <typename T, typename F>
std::vector<T> Node::getPlural(const std::string& singular,
                               const std::string& plural,
                               F&& f) const {
    std::vector<T> out;
    const auto& pluralValue = (*this)[plural];
    const auto& singValue = (*this)[singular];
    if (pluralValue && singValue) {
        BOOST_THROW_EXCEPTION(
        // the `$plural(singular,plural)` key is kinda cheeky but hopefully
        // it helps to explain what the code tried to do in error-messages.
                InvalidKeyException("Can't have both '" + singular + "' and '" + plural + "'.",
                                    "getPlural('" + singular + "', '" + plural + "')",
                                    this));
    } else if (pluralValue) {
        if (!pluralValue.isSequence()) {
            BOOST_THROW_EXCEPTION(
                    InvalidKeyException("Plural '" + plural + "' must be a sequence type.",
                                        "getPlural('" + singular + "', '" + plural + "')",
                                        this));
        }
        for (auto&& [k,v] : pluralValue) {
            auto&& created = std::invoke(f, v);
            out.emplace_back(std::move(created));
        }
    } else if (singValue) {
        auto&& created = std::invoke(f, singValue);
        out.emplace_back(std::move(created));
    } else if (!singValue && !pluralValue) {
        BOOST_THROW_EXCEPTION(
                InvalidKeyException("Either '" + singular + "' or '" + plural + "' required.",
                                    "getPlural('" + singular + "', '" + plural + "')",
                                    this));
    }
    return out;
}


#endif  // HEADER_17681835_40A0_443E_939D_3679A1A6B5DD_INCLUDED
