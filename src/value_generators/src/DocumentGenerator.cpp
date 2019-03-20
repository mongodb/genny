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

#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <map>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny {

namespace {

std::string toString(const YAML::Node& node) {
    YAML::Emitter e;
    e << node;
    return std::string{e.c_str()};
}

YAML::Node extract(YAML::Node node, std::string key, std::string msg) {
    auto out = node[key];
    if (!out) {
        std::stringstream ex;
        ex << "Missing '" << key << "' for " << msg << " in input " << toString(node);
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(ex.str()));
    }
    return out;
}

const std::string kDefaultAlphabet = std::string{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"};

}  // namespace

class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;
};


using UniqueAppendable = std::unique_ptr<Appendable>;
using UniqueInt64Generator = std::unique_ptr<Int64Generator::Impl>;
using UniqueInt32Generator = std::unique_ptr<Int64Generator::Impl>;
using UniqueStringGenerator = std::unique_ptr<StringGenerator::Impl>;

UniqueInt64Generator randomInt64Operand(YAML::Node node, DefaultRandom &rng);
UniqueInt64Generator int64Generator(YAML::Node node, DefaultRandom &rng);
UniqueStringGenerator fastRandomStringOperand(YAML::Node node, DefaultRandom &rng);
UniqueStringGenerator randomStringOperand(YAML::Node node, DefaultRandom &rng);
UniqueAppendable verbatimOperand(YAML::Node node, DefaultRandom &rng);

using UniqueArrayGenerator = std::unique_ptr<ArrayGenerator::Impl>;
using UniqueDocumentGenerator = std::unique_ptr<DocumentGenerator::Impl>;

template<bool Verbatim>
UniqueDocumentGenerator documentGenerator(YAML::Node node, DefaultRandom& rng);

template<typename O>
using Parser = std::function<O(YAML::Node, DefaultRandom&)>;

template<bool Verbatim>
UniqueArrayGenerator arrayGenerator(YAML::Node node, DefaultRandom& rng);

static std::map<std::string, Parser<UniqueInt64Generator>> intParsers {
        {"^RandomInt", randomInt64Operand},
};


/////////////////////////////////////////////////////////////////////////////////////////



template<typename T>
class ConstantAppender : public Appendable {
public:
    explicit ConstantAppender(T value)
    : _value{value} {}
    explicit ConstantAppender()
    : _value{} {}
    T evaluate() {
        return _value;
    }
    ~ConstantAppender() override = default;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, _value));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(_value);
    }
protected:
    T _value;
};


class Int64Generator::Impl : public Appendable {
public:
    virtual int64_t evaluate() = 0;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }

    ~Impl() override = default;
};


class Int32Generator::Impl : public Appendable {
public:
    virtual int32_t evaluate() = 0;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }

    ~Impl() override = default;
};

class UniformInt64Generator : public Int64Generator::Impl {
public:
    /**
     * @param node {min:<int>, max:<int>}
     */
    UniformInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _minGen{int64Generator(extract(node, "min", "uniform"), _rng)},
          _maxGen{int64Generator(extract(node, "max", "uniform"), _rng)} {}
    ~UniformInt64Generator() override = default;

    int64_t evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        auto distribution = std::uniform_int_distribution<int64_t>{min, max};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    UniqueInt64Generator _minGen;
    UniqueInt64Generator _maxGen;
};

class BinomialInt64Generator : public Int64Generator::Impl {
public:
    /**
     * @param node {t:<int>, p:double}
     */
    BinomialInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _tGen{int64Generator(extract(node, "t", "binomial"), _rng)},
          _p{extract(node, "p", "binomial").as<double>()} {}
    ~BinomialInt64Generator() override = default;

    int64_t evaluate() override {
        auto distribution = std::binomial_distribution<int64_t>{_tGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueInt64Generator _tGen;
};

class NegativeBinomialInt64Generator : public Int64Generator::Impl {
public:
    /**
     * @param node {k:<int>, p:double}
     */
    NegativeBinomialInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _kGen{int64Generator(extract(node, "k", "negative_binomial"), _rng)},
          _p{extract(node, "p", "negative_binomial").as<double>()} {}
    ~NegativeBinomialInt64Generator() override = default;

    int64_t evaluate() override {
        auto distribution = std::negative_binomial_distribution<int64_t>{_kGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueInt64Generator _kGen;
};

class PoissonInt64Generator : public Int64Generator::Impl {
public:
    /**
     * @param node {mean:double}
     */
    PoissonInt64Generator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng}, _mean{extract(node, "mean", "poisson").as<double>()} {}
    ~PoissonInt64Generator() override = default;

    int64_t evaluate() override {
        auto distribution = std::poisson_distribution<int64_t>{_mean};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _mean;
};

class GeometricInt64Generator : public Int64Generator::Impl {
public:
    /**
     * @param node {mean:double}
     */
    GeometricInt64Generator(YAML::Node node, DefaultRandom& rng)
            : _rng{rng}, _p{extract(node, "p", "geometric").as<double>()} {}
    ~GeometricInt64Generator() override = default;

    int64_t evaluate() override {
        auto distribution = std::geometric_distribution<int64_t>{_p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
};

class ConstantInt64Generator : public Int64Generator::Impl {
public:
    ConstantInt64Generator(int64_t value)
    : _value{value} {}
    ~ConstantInt64Generator() override = default;

    int64_t evaluate() override {
        return _value;
    }
private:
    int64_t _value;
};

class StringGenerator::Impl : public Appendable {
public:
    virtual std::string evaluate() = 0;

    ~Impl() override = default;

    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }
};


class NormalRandomStringGenerator : public StringGenerator::Impl {
public:
    /**
     * @param node {length:<int>, alphabet:opt string}
     */
    NormalRandomStringGenerator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _lengthGen{int64Generator(extract(node, "length", "^RandomString"), rng)},
          _alphabet{node["alphabet"].as<std::string>(kDefaultAlphabet)},
          _alphabetLength{_alphabet.size()} {
            if (_alphabetLength <= 0) {
                BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Random string requires non-empty alphabet if specified"));
            }
          }

    ~NormalRandomStringGenerator() override = default;

    std::string evaluate() override {
        auto distribution = std::uniform_int_distribution<size_t>{0, _alphabetLength - 1};

        auto length = _lengthGen->evaluate();
        std::string str(length, '\0');

        for (int i = 0; i < length; ++i) {
            str[i] = _alphabet[distribution(_rng)];
        }

        return str;
    }

private:
    DefaultRandom& _rng;
    UniqueInt64Generator _lengthGen;
    std::string _alphabet;
    size_t _alphabetLength;
};

class FastRandomStringGenerator : public StringGenerator::Impl {
public:
    /**
     * @param node {length:<int>, alphabet:opt str}
     */
    FastRandomStringGenerator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _lengthGen{int64Generator(extract(node, "length", "^FastRandomString"), rng)},
          _alphabet{node["alphabet"].as<std::string>(kDefaultAlphabet)},
          _alphabetLength{_alphabet.size()} {
        if (_alphabetLength <= 0) {
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Random string requires non-empty alphabet if specified"));
        }
    }

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

private:
    DefaultRandom& _rng;
    UniqueInt64Generator _lengthGen;
    std::string _alphabet;
    size_t _alphabetLength;
};

class ArrayGenerator::Impl : public Appendable {
public:
    virtual bsoncxx::array::value evaluate() = 0;
    ~Impl() override = default;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }
};


class NormalArrayGenerator : public ArrayGenerator::Impl {
public:
    using ValueType = std::vector<UniqueAppendable>;
    explicit NormalArrayGenerator(ValueType values)
    : _values{std::move(values)} {}
    ~NormalArrayGenerator() override = default;

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        for (auto&& value : _values) {
            value->append(builder);
        }
        return builder.extract();
    }

private:
    ValueType _values;
};

class DocumentGenerator::Impl : public Appendable {
public:
    virtual bsoncxx::document::value evaluate() = 0;

    ~Impl() override = default;

    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }
};

class NormalDocumentGenerator : public DocumentGenerator::Impl {
public:
    // order matters for comparison in tests; std::map is ordered
    using Entries = std::vector<std::pair<std::string,UniqueAppendable>>;
    ~NormalDocumentGenerator() override = default;
    explicit NormalDocumentGenerator(Entries entries)
    : _entries{std::move(entries)} {}

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

DocumentGenerator DocumentGenerator::create(YAML::Node node, DefaultRandom& rng) {
    return DocumentGenerator{node, rng};
}

DocumentGenerator::DocumentGenerator(YAML::Node node, DefaultRandom& rng)
    : _impl{documentGenerator<false>(node, rng)} {}

bsoncxx::document::value DocumentGenerator::operator()() {
    return _impl->evaluate();
}

DocumentGenerator::DocumentGenerator(DocumentGenerator &&) noexcept = default;

DocumentGenerator::~DocumentGenerator() = default;

UniqueInt64Generator randomInt64Operand(YAML::Node node, DefaultRandom &rng) {
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

UniqueStringGenerator fastRandomStringOperand(YAML::Node node, DefaultRandom &rng) {
    return std::make_unique<FastRandomStringGenerator>(node, rng);
}

UniqueStringGenerator randomStringOperand(YAML::Node node, DefaultRandom &rng) {
    return std::make_unique<NormalRandomStringGenerator>(node, rng);
}

std::optional<std::string> getMetaKey(YAML::Node node) {
    size_t foundKeys = 0;
    std::optional<std::string> out = std::nullopt;
    for(const auto&& kvp : node) {
        ++foundKeys;
        auto key = kvp.first.as<std::string>();
        if (!key.empty() && key[0] == '^') {
            out = key;
        }
    }
    if (foundKeys > 1 && out) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Found multiple meta-keys"));
    }
    return out;
}

template<typename O>
std::optional<std::pair<Parser<O>, std::string>> extractKnownParser(YAML::Node node, DefaultRandom& rng,
                                            std::map<std::string, Parser<O>> parsers) {
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

    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Unknown parser"));
}

template<bool Verbatim, typename Out>
Out valueGenerator(YAML::Node node, DefaultRandom &rng, const std::map<std::string,Parser<Out>>& parsers) {
    if constexpr (!Verbatim) {
        if (auto parserPair = extractKnownParser(node, rng, parsers)) {
            // known parser type
            return parserPair->first(node[parserPair->second], rng);
        }
    }

    if (!node) {
        BOOST_THROW_EXCEPTION(std::logic_error("Unknown node"));
    }
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
    BOOST_THROW_EXCEPTION(std::logic_error("Unknown node type"));
}

static std::map<std::string, Parser<UniqueAppendable>> allParsers {
        {"^FastRandomString", fastRandomStringOperand},
        {"^RandomString", randomStringOperand},
        {"^RandomInt", randomInt64Operand},
        {"^Verbatim", verbatimOperand},
};

template<bool Verbatim>
UniqueDocumentGenerator documentGenerator(YAML::Node node, DefaultRandom& rng) {
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

    NormalDocumentGenerator::Entries entries;
    for(const auto&& ent : node) {
        auto key = ent.first.as<std::string>();
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(ent.second, rng, allParsers);
        entries.emplace_back(key, std::move(valgen));
    }
    return std::make_unique<NormalDocumentGenerator>(std::move(entries));
}

template<bool Verbatim>
UniqueArrayGenerator arrayGenerator(YAML::Node node, DefaultRandom& rng) {
    NormalArrayGenerator::ValueType entries;
    for(const auto&& ent : node) {
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(ent, rng, allParsers);
        entries.push_back(std::move(valgen));
    }
    return std::make_unique<NormalArrayGenerator>(std::move(entries));
}

UniqueAppendable verbatimOperand(YAML::Node node, DefaultRandom &rng) {
    return valueGenerator<true, UniqueAppendable>(node, rng, allParsers);
}


UniqueInt64Generator int64Generator(YAML::Node node, DefaultRandom &rng) {
    if (auto parserPair = extractKnownParser(node, rng, intParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], rng);
    }
    return std::make_unique<ConstantInt64Generator>(node.as<int64_t>());
}

}  // namespace genny
