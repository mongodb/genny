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

#include <value_generators/DocumentGenerator.hpp>

#include <functional>
#include <map>
#include <sstream>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

namespace {

class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;
};

using UniqueAppendable = std::unique_ptr<Appendable>;

template <class T>
class Generator : public Appendable {
public:
    virtual ~Generator() = default;
    virtual T evaluate() = 0;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }
};

template <class T>
using UniqueGenerator = std::unique_ptr<Generator<T>>;

template <typename T>
class ConstantAppender : public Generator<T> {
public:
    explicit ConstantAppender(T value) : _value{value} {}
    explicit ConstantAppender() : _value{} {}
    T evaluate() override {
        return _value;
    }

protected:
    T _value;
};

}  // namespace


namespace genny {

class DocumentGenerator::Impl : public Generator<bsoncxx::document::value> {
public:
    using Entries = std::vector<std::pair<std::string, UniqueAppendable>>;

    explicit Impl(Entries entries) : _entries{std::move(entries)} {}

    bsoncxx::document::value evaluate() override {
        bsoncxx::builder::basic::document builder;
        for (auto&& [k, app] : _entries) {
            app->append(k, builder);
        }
        return builder.extract();
    }

private:
    Entries _entries;
};

namespace {

/**
 * @param node
 *  a yaml node
 * @return
 *   a string representation (in standard yaml formatting) of the node
 */
std::string toString(const YAML::Node& node) {
    YAML::Emitter e;
    e << node;
    return std::string{e.c_str()};
}

/**
 * Extract a required key from a node or barf if not there
 * @param node
 *   haystack to look in
 * @param key
 *   needle key to look for
 * @param src
 *   useful source location included in the exception message
 * @return
 *   `node[key]` if exists else throw with a meaningful error message
 */
YAML::Node extract(YAML::Node node, const std::string& key, std::string src) {
    auto out = node[key];
    if (!out) {
        std::stringstream ex;
        ex << "Missing '" << key << "' for '" << src << "' in input " << toString(node);
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(ex.str()));
    }
    return out;
}

/**
 * @param node
 *   a "sub"-structure e.g. should get `{v}` in `{a:{v}`
 * @return
 *   the single ^-prefixed key if there is exactly one
 *   ^-prefixed key. If there is a ^-prefixed keys
 *   *and* any additional keys then barf.
 */
std::optional<std::string> getMetaKey(YAML::Node node) {
    size_t foundKeys = 0;
    std::optional<std::string> out = std::nullopt;
    for (const auto&& kvp : node) {
        ++foundKeys;
        auto key = kvp.first.as<std::string>();
        if (!key.empty() && key[0] == '^') {
            out = key;
        }
        if (foundKeys > 1 && out) {
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Found multiple meta-keys"));
        }
    }
    return out;
}

/** Default alphabet for string generators */
static const std::string kDefaultAlphabet = std::string{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"};

// Useful typedefs

template <typename O>
using Parser = std::function<O(YAML::Node, DefaultRandom&)>;

// Pre-declaring all at once
// Documentation is at the implementations-site.

UniqueGenerator<int64_t> intGenerator(YAML::Node node, DefaultRandom& rng);
UniqueGenerator<int64_t> int64GeneratorBasedOnDistribution(YAML::Node node, DefaultRandom& rng);

template <bool Verbatim>
std::unique_ptr<DocumentGenerator::Impl> documentGenerator(YAML::Node node, DefaultRandom& rng);

template <bool Verbatim>
UniqueGenerator<bsoncxx::array::value> arrayGenerator(YAML::Node node, DefaultRandom& rng);

/** `{^RandomInt:{distribution:uniform ...}}` */
class UniformInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{min:<int>, max:<int>}` */
    UniformInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _minGen{intGenerator(extract(node, "min", "uniform"), _rng)},
          _maxGen{intGenerator(extract(node, "max", "uniform"), _rng)} {}

    int64_t evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        auto distribution = std::uniform_int_distribution<int64_t>{min, max};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    UniqueGenerator<int64_t> _minGen;
    UniqueGenerator<int64_t> _maxGen;
};

/** `{^RandomInt:{distribution:binomial ...}}` */
class BinomialInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{t:<int>, p:double}` */
    BinomialInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _tGen{intGenerator(extract(node, "t", "binomial"), _rng)},
          _p{extract(node, "p", "binomial").as<double>()} {}

    int64_t evaluate() override {
        auto distribution = std::binomial_distribution<int64_t>{_tGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueGenerator<int64_t> _tGen;
};

/** `{^RandomInt:{distribution:negative_binomial ...}}` */
class NegativeBinomialInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{k:<int>, p:double}` */
    NegativeBinomialInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _kGen{intGenerator(extract(node, "k", "negative_binomial"), _rng)},
          _p{extract(node, "p", "negative_binomial").as<double>()} {}

    int64_t evaluate() override {
        auto distribution = std::negative_binomial_distribution<int64_t>{_kGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueGenerator<int64_t> _kGen;
};

/** `{^RandomInt:{distribution:poisson...}}` */
class PoissonInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{mean:double}` */
    PoissonInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng}, _mean{extract(node, "mean", "poisson").as<double>()} {}

    int64_t evaluate() override {
        auto distribution = std::poisson_distribution<int64_t>{_mean};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _mean;
};

/** `{^RandomInt:{distribution:geometric...}}` */
class GeometricInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{mean:double}` */
    GeometricInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng}, _p{extract(node, "p", "geometric").as<double>()} {}

    int64_t evaluate() override {
        auto distribution = std::geometric_distribution<int64_t>{_p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    const double _p;
};


class StringGenerator : public Generator<std::string> {
public:
    StringGenerator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _lengthGen{intGenerator(extract(node, "length", "^RandomString"), rng)},
          _alphabet{node["alphabet"].as<std::string>(kDefaultAlphabet)},
          _alphabetLength{_alphabet.size()} {
        if (_alphabetLength <= 0) {
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(
                "Random string requires non-empty alphabet if specified"));
        }
    }

protected:
    DefaultRandom& _rng;
    UniqueGenerator<int64_t> _lengthGen;
    const std::string _alphabet;
    const size_t _alphabetLength;
};

/** `{^RandomString:{...}` */
class NormalRandomStringGenerator : public StringGenerator {
public:
    /**
     * @param node `{length:<int>, alphabet:opt string}`
     */
    NormalRandomStringGenerator(YAML::Node node, DefaultRandom& rng) : StringGenerator(node, rng) {}

    std::string evaluate() override {
        auto distribution = std::uniform_int_distribution<size_t>{0, _alphabetLength - 1};

        auto length = _lengthGen->evaluate();
        std::string str(length, '\0');

        for (int i = 0; i < length; ++i) {
            str[i] = _alphabet[distribution(_rng)];
        }

        return str;
    }
};

/** `{^FastRandomString:{...}` */
class FastRandomStringGenerator : public StringGenerator {
public:
    /** @param node `{length:<int>, alphabet:opt str}` */
    FastRandomStringGenerator(YAML::Node node, DefaultRandom& rng) : StringGenerator(node, rng) {}

    std::string evaluate() override {
        auto length = _lengthGen->evaluate();
        std::string str(length, '\0');

        auto randomValue = _rng();
        int bits = 64;

        for (int i = 0; i < length; ++i) {
            if (bits < 6) {
                randomValue = _rng();
                bits = 64;
            }

            str[i] = _alphabet[(randomValue & 0x2f) % _alphabetLength];
            randomValue >>= 6;
            bits -= 6;
        }
        return str;
    }
};

/** `{a: [...]}` */
class ArrayGenerator : public Generator<bsoncxx::array::value> {
public:
    using ValueType = std::vector<UniqueAppendable>;

    explicit ArrayGenerator(ValueType values) : _values{std::move(values)} {}

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        for (auto&& value : _values) {
            value->append(builder);
        }
        return builder.extract();
    }

private:
    const ValueType _values;
};

/**
 * @tparam O
 *   parser output type
 * @param node
 *   a sub-field e.g. {^RandomInt:{...}} or a scalar.
 * @param parsers
 *   Which parsers (all of type Parser<O>) to use.
 * @return Parser<O>
 *   if the document reprents a evaluate-able structure.
 *   E.g. returns the randomInt parser if node is {^RandomInt:{...}}
 *   or nullopt if node is a scalar etc. Throws if node looks
 *   evaluate-able but e.g. has two ^-prefixed keys or if the ^-prefixed
 *   key isn't in the list of parsers.
 */
template <typename O>
std::optional<std::pair<Parser<O>, std::string>> extractKnownParser(
    YAML::Node node, DefaultRandom& rng, std::map<std::string, Parser<O>> parsers) {
    if (!node || !node.IsMap()) {
        return std::nullopt;
    }

    auto metaKey = getMetaKey(node);
    if (!metaKey) {
        return std::nullopt;
    }

    if (auto parser = parsers.find(*metaKey); parser != parsers.end()) {
        return std::make_optional<std::pair<Parser<O>, std::string>>({parser->second, *metaKey});
    }

    std::stringstream msg;
    msg << "Unknown parser '" << *metaKey << "' in node '" << toString(node) << "'";
    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
}

/**
 * Main entry-point into depth-first construction.
 *
 * @tparam Verbatim if we're in a ^Verbatim node
 * @tparam Out desired Parser output type
 * @param node any type of node e.g. {a:1}, {^RandomInt:{...}}, [{a:1},{^RandomInt}]
 * @param parsers must all be of type Parser<O>
 * @return the indicated parser
 */
template <bool Verbatim, typename Out>
Out valueGenerator(YAML::Node node,
                   DefaultRandom& rng,
                   const std::map<std::string, Parser<Out>>& parsers) {
    if constexpr (!Verbatim) {
        if (auto parserPair = extractKnownParser(node, rng, parsers)) {
            // known parser type
            return parserPair->first(node[parserPair->second], rng);
        }
    }
    // switch-statement on node.Type() may be clearer

    if (node.IsNull()) {
        return std::make_unique<ConstantAppender<bsoncxx::types::b_null>>();
    }
    if (node.IsScalar()) {
        if (node.Tag() != "!") {
            try {
                return std::make_unique<ConstantAppender<int32_t>>(node.as<int32_t>());
            } catch (const YAML::BadConversion& e) {
            }
            try {
                return std::make_unique<ConstantAppender<int64_t>>(node.as<int64_t>());
            } catch (const YAML::BadConversion& e) {
            }
            try {
                return std::make_unique<ConstantAppender<double>>(node.as<double>());
            } catch (const YAML::BadConversion& e) {
            }
            try {
                return std::make_unique<ConstantAppender<bool>>(node.as<bool>());
            } catch (const YAML::BadConversion& e) {
            }
        }
        return std::make_unique<ConstantAppender<std::string>>(node.as<std::string>());
    }
    if (node.IsSequence()) {
        return arrayGenerator<Verbatim>(node, rng);
    }
    if (node.IsMap()) {
        return documentGenerator<Verbatim>(node, rng);
    }

    std::stringstream msg;
    msg << "Malformed node " << toString(node);
    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
}

const static std::map<std::string, Parser<UniqueAppendable>> allParsers{
    {"^FastRandomString",
     [](YAML::Node node, DefaultRandom& rng) {
         return std::make_unique<FastRandomStringGenerator>(node, rng);
     }},
    {"^RandomString",
     [](YAML::Node node, DefaultRandom& rng) {
         return std::make_unique<NormalRandomStringGenerator>(node, rng);
     }},
    {"^RandomInt", int64GeneratorBasedOnDistribution},
    {"^Verbatim",
     [](YAML::Node node, DefaultRandom& rng) {
         return valueGenerator<true, UniqueAppendable>(node, rng, allParsers);
     }},
};


/**
 * Used for top-level values that are of type Map.
 * @tparam Verbatim if we are in a `^Verbatim` block
 * @param node a "top-level"-like node e.g. `{a:1, b:{^RandomInt:{...}}`
 */
template <bool Verbatim>
std::unique_ptr<DocumentGenerator::Impl> documentGenerator(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Must be mapping type"));
    }
    if constexpr (!Verbatim) {
        auto meta = getMetaKey(node);
        if (meta) {
            if (meta == "^Verbatim") {
                return documentGenerator<true>(node["^Verbatim"], rng);
            }
            std::stringstream msg;
            msg << "Invalid meta-key " << *meta << " at top-level";
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
    }

    DocumentGenerator::Impl::Entries entries;
    for (const auto&& ent : node) {
        auto key = ent.first.as<std::string>();
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(ent.second, rng, allParsers);
        entries.emplace_back(key, std::move(valgen));
    }
    return std::make_unique<DocumentGenerator::Impl>(std::move(entries));
}

/**
 * @tparam Verbatim if we're in a `^Verbatim block`
 * @param node sequence node
 * @return array generator that has one valueGenerator (recursive type) for each element in the node
 */
template <bool Verbatim>
UniqueGenerator<bsoncxx::array::value> arrayGenerator(YAML::Node node, DefaultRandom& rng) {
    ArrayGenerator::ValueType entries;
    for (const auto&& ent : node) {
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(ent, rng, allParsers);
        entries.push_back(std::move(valgen));
    }
    return std::make_unique<ArrayGenerator>(std::move(entries));
}


/**
 * @param node
 *   the *value* from a `^RandomInt` node.
 *   E.g. if higher-up has `{^RandomInt:{v}}`, this will have `node={v}`
 */
//
// We need this additional lookup function for int64s (but not for other types)
// because we do "double-dispatch" for ^RandomInt. So int64Operand determines
// if we're looking at ^RandomInt or a constant. If we're looking at ^RandomInt
// it dispatches to here to determine which Int64Generator to use.
//
// An alternative would have been to have ^RandomIntUniform etc.
//
UniqueGenerator<int64_t> int64GeneratorBasedOnDistribution(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("random int must be given mapping type"));
    }
    auto distribution = node["distribution"].as<std::string>("uniform");

    if (distribution == "uniform") {
        return std::make_unique<UniformInt64Generator>(node, rng);
    } else if (distribution == "binomial") {
        return std::make_unique<BinomialInt64Generator>(node, rng);
    } else if (distribution == "negative_binomial") {
        return std::make_unique<NegativeBinomialInt64Generator>(node, rng);
    } else if (distribution == "poisson") {
        return std::make_unique<PoissonInt64Generator>(node, rng);
    } else if (distribution == "geometric") {
        return std::make_unique<GeometricInt64Generator>(node, rng);
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(error.str()));
    }
}

/**
 * @param node
 *   a top-level document value i.e. either a scalar or a `^RandomInt` value
 * @return
 *   either a `^RantomInt` generator (etc--see `intParsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<int64_t> intGenerator(YAML::Node node, DefaultRandom& rng) {
    // Set of parsers to look when we request an int parser
    // see int64Generator
    const static std::map<std::string, Parser<UniqueGenerator<int64_t>>> intParsers{
        {"^RandomInt", int64GeneratorBasedOnDistribution},
    };

    if (auto parserPair = extractKnownParser(node, rng, intParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], rng);
    }
    return std::make_unique<ConstantAppender<int64_t>>(node.as<int64_t>());
}

}  // namespace


// Pass-through for ctor. Can be removed in favor of just calling the ctor?
DocumentGenerator DocumentGenerator::create(YAML::Node node, DefaultRandom& rng) {
    return DocumentGenerator{node, rng};
}

// Kick the recursion into motion
DocumentGenerator::DocumentGenerator(YAML::Node node, DefaultRandom& rng)
    : _impl{documentGenerator<false>(node, rng)} {}

DocumentGenerator::DocumentGenerator(DocumentGenerator&&) noexcept = default;

DocumentGenerator::~DocumentGenerator() = default;

// Can't define this before DocumentGenerator::Impl ↑
bsoncxx::document::value DocumentGenerator::operator()() {
    return _impl->evaluate();
}

}  // namespace genny
