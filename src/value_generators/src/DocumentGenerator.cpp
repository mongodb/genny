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

#include <iostream>
#include <cstdlib>
#include <functional>
#include <random>
#include <sstream>
#include <unordered_map>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <value_generators/DocumentGenerator.hpp>


// TODO: remove
std::string toString(const YAML::Node& node) {
    YAML::Emitter e;
    e << node;
    return std::string{e.c_str()};
}

namespace genny {

/////////////////////////////////////////////////////
// Appendable
/////////////////////////////////////////////////////


class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;
};

using UniqueAppendable = std::unique_ptr<Appendable>;

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng);

////////////////////////////////////////////////
// Null
////////////////////////////////////////////////

class NullGenerator : public Appendable {
public:
    NullGenerator(YAML::Node, DefaultRandom&) {}
    ~NullGenerator() override = default;
    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, bsoncxx::types::b_null{}));
    }
    void append(bsoncxx::builder::basic::array &builder) override {
        builder.append(bsoncxx::types::b_null{});
    }
};


////////////////////////////////////////////////
// Int Generators
////////////////////////////////////////////////


class IntGenerator::Impl : public Appendable {
public:
    virtual int64_t evaluate() = 0;
    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array &builder) override {
        builder.append(this->evaluate());
    }
    virtual ~Impl() = default;
};

using UniqueIntGenerator = std::unique_ptr<IntGenerator::Impl>;

UniqueIntGenerator randomInt(YAML::Node node, DefaultRandom &rng);

YAML::Node extract(YAML::Node node, std::string key, std::string msg) {
    auto out = node[key];
    if (!out) {
        std::stringstream ex;
        ex << "Missing '" << key << "' for " << msg << " in input " << toString(node);
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(ex.str()));
    }
    return out;
}

class UniformIntGenerator : public IntGenerator::Impl {
public:
    UniformIntGenerator(YAML::Node node, DefaultRandom& rng)
        : _rng{rng},
          _minGen{randomInt(extract(node, "min", "uniform"), _rng)},
          _maxGen{randomInt(extract(node, "max", "uniform"), _rng)} {}
    ~UniformIntGenerator() override = default;

    int64_t evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        auto distribution = std::uniform_int_distribution<int64_t>{min, max};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    UniqueIntGenerator _minGen;
    UniqueIntGenerator _maxGen;
};


class BinomialIntGenerator : public IntGenerator::Impl {
public:
    BinomialIntGenerator(YAML::Node node, DefaultRandom& rng)
            : _rng{rng},
              _tGen{randomInt(extract(node, "t", "binomial"), _rng)},
              _p{extract(node, "p", "binomial").as<double>()} {}
    ~BinomialIntGenerator() override = default;

    int64_t evaluate() override {
        auto distribution = std::binomial_distribution<int64_t>{_tGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueIntGenerator _tGen;
};

class NegativeBinomialIntGenerator : public IntGenerator::Impl {
public:
    NegativeBinomialIntGenerator(YAML::Node node, DefaultRandom& rng)
            : _rng{rng},
              _kGen{randomInt(extract(node, "k", "negative_binomial"), _rng)},
              _p{extract(node, "p", "negative_binomial").as<double>()} {}
    ~NegativeBinomialIntGenerator() override = default;

    int64_t evaluate() override {
        auto distribution = std::negative_binomial_distribution<int64_t>{_kGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _p;
    UniqueIntGenerator _kGen;
};

class PoissonIntGenerator : public IntGenerator::Impl {
public:
    PoissonIntGenerator(YAML::Node node, DefaultRandom& rng)
            : _rng{rng},
              _mean{extract(node, "mean", "poisson").as<double>()} {}
    ~PoissonIntGenerator() override = default;

    int64_t evaluate() override {
        auto distribution = std::poisson_distribution<int64_t>{_mean};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    double _mean;
};



class ConstantIntGenerator : public IntGenerator::Impl {
public:
    ConstantIntGenerator(YAML::Node node, DefaultRandom&) : _value{node.as<int64_t>()} {}
    ~ConstantIntGenerator() override = default;

    int64_t evaluate() override {
        return _value;
    }

private:
    int64_t _value;
};



/////////////////////////////////////////////////////
// String Generators
/////////////////////////////////////////////////////

class StringGenerator::Impl : public Appendable {
    friend class ConstantStringGenerator;

public:
    virtual std::string evaluate() = 0;
    virtual ~Impl() = default;

    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array &builder) override {
        builder.append(this->evaluate());
    }
};

using UniqueStringGenerator = std::unique_ptr<StringGenerator::Impl>;

UniqueStringGenerator pickStringGenerator(YAML::Node node, DefaultRandom &rng);

class ConstantStringGenerator : public StringGenerator::Impl {
public:
    ConstantStringGenerator(YAML::Node node, DefaultRandom&)
    : _value{node.as<std::string>()} {}
    ~ConstantStringGenerator() = default;

    std::string evaluate() override {
        return _value;
    }

private:
    std::string _value;
};


class NormalRandomStringGenerator : public StringGenerator::Impl {
private:
    const static std::string kDefaultAlphabet;

public:
    NormalRandomStringGenerator(YAML::Node node, DefaultRandom& rng)
    : _rng{rng},
      _lengthGen{randomInt(extract(node, "length", "^RandomString"), rng)},
      _alphabet{node["alphabet"].as<std::string>(kDefaultAlphabet)},
      _alphabetLength{_alphabet.size()} {}

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
    UniqueIntGenerator _lengthGen;
    std::string _alphabet;
    size_t _alphabetLength;
};

const std::string NormalRandomStringGenerator::kDefaultAlphabet  = std::string{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/"
};

/////////////////////////////////////////////////////
// Array Generator
/////////////////////////////////////////////////////

class ArrayGenerator::Impl : public Appendable {
    friend class NormalArrayGenerator;
public:
    virtual bsoncxx::array::value evaluate() = 0;
    virtual ~Impl() = default;
    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array &builder) override {
        builder.append(this->evaluate());
    }
};

using UniqueArrayGenerator= std::unique_ptr<ArrayGenerator::Impl>;

UniqueArrayGenerator pickArrayGenerator(YAML::Node node, DefaultRandom &rng);

class NormalArrayGenerator : public ArrayGenerator::Impl {
public:
    NormalArrayGenerator(YAML::Node node, DefaultRandom&) {}
    ~NormalArrayGenerator() = default;

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder {};
        for(auto&& value : _values) {
            value->append(builder);
        }
        return builder.extract();
    }

private:
    std::vector<UniqueAppendable> _values;
};


/////////////////////////////////////////////////////
// Document Generator
//////////////////////////////////////////////////////

class DocumentGenerator::Impl : public Appendable {
    friend class NormalDocumentGenerator;
    friend class ConstantDocumentGenerator;

public:
    virtual bsoncxx::document::value evaluate() = 0;
    virtual ~Impl() = default;

    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array &builder) override {
        builder.append(this->evaluate());
    }
};

using UniqueDocumentGenerator = std::unique_ptr<DocumentGenerator::Impl>;

class NormalDocumentGenerator : public DocumentGenerator::Impl {

public:
    using ElementType = std::pair<std::string, UniqueAppendable>;
    ~NormalDocumentGenerator() = default;
    NormalDocumentGenerator(YAML::Node node, DefaultRandom& rng) {
        // will always be a map but may be e.g. {^RandomInt:} or {a:7}
        if (!node.IsMap()) {
            throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
        }

        auto elements = std::vector<ElementType>{};
        bool hasMetaKey = false;
        for (auto&& entry : node) {
            if (entry.first.as<std::string>()[0] == '^') {
                if (hasMetaKey) {
                    // TODO: bad message
                    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Invalid use of meta-key"));
                }
                hasMetaKey = true;
            }
            elements.emplace_back(entry.first.as<std::string>(), pickAppendable(entry.second, rng));
        }
        _elements = std::move(elements);
    }

    bsoncxx::document::value evaluate() override {
        bsoncxx::builder::basic::document builder;
        for(auto&& [k,app] : _elements) {
            app->append(k, builder);
        }
        return builder.extract();
    }

private:
    std::vector<ElementType> _elements;
};

DocumentGenerator DocumentGenerator::create(YAML::Node node, DefaultRandom& rng) {
    return DocumentGenerator{node, rng};
}

DocumentGenerator::DocumentGenerator(YAML::Node node, DefaultRandom& rng)
        : _impl{std::make_unique<NormalDocumentGenerator>(node, rng)} {}

bsoncxx::document::value DocumentGenerator::operator()() {
    return _impl->evaluate();
}

DocumentGenerator::~DocumentGenerator() = default;

//////////////////////////////////////
// Pick impls
//////////////////////////////////////

//const auto parserMap = std::unordered_map<std::string, Parser>{
//    {"^FastRandomString", fastRandomString},
//    {"^RandomInt", randomInt},
//    {"^RandomString", randomString},
//    {"^Verbatim", verbatim}};

///*
//std::map<std::string, decltype(pickIntAppendable)> parserMap {
//        {"^RandomInt", pickIntAppendable},
//};
//
//*/

using Parser = std::function<UniqueAppendable(YAML::Node, DefaultRandom &)>;

UniqueIntGenerator randomInt(YAML::Node node, DefaultRandom &rng) {
    // gets the value from a kvp. Gets value from either {a:7} (gets 7)
    // or {a:{^RandomInt:{distribution...}} gets {distribution...}
    if (node.IsScalar()) {
        return std::make_unique<ConstantIntGenerator>(node, rng);
    }

    if (node.IsSequence()) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Got sequence"));
    }

    auto distribution = node["distribution"].as<std::string>("uniform");

    if (distribution == "uniform") {
        return std::make_unique<UniformIntGenerator>(node, rng);
    } else if ( distribution == "binomial") {
        return std::make_unique<BinomialIntGenerator>(node, rng);
    } else if (distribution == "negative_binomial") {
        return std::make_unique<NegativeBinomialIntGenerator>(node, rng);
    } else if (distribution == "poisson") {
    return std::make_unique<PoissonIntGenerator>(node, rng);
    }
    else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }
}

UniqueStringGenerator fastRandomString(YAML::Node node, DefaultRandom &rng) {
    // TODO
    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("fastRandomString"));
}

UniqueStringGenerator randomString(YAML::Node node, DefaultRandom &rng) {
    return std::make_unique<NormalRandomStringGenerator>(node, rng);
}

UniqueDocumentGenerator verbatim(YAML::Node node, DefaultRandom &rng) {
    // TODO
    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("verbatim"));
}

template<typename O>
using UParser = std::function<O(YAML::Node, DefaultRandom &)>;

static std::map<std::string, UParser<UniqueStringGenerator>> stringParsers {
        {"^FastRandomString", fastRandomString},
        {"^RandomString", randomString},
};

static std::map<std::string, UParser<UniqueIntGenerator>> intParsers {
    {"^RandomInt", randomInt},
};

static std::map<std::string, UParser<UniqueDocumentGenerator>> docParsers {
        {"^Verbatim", verbatim}
};



template<class O, class DefaultIfNotFound>
O extractOrConstant(YAML::Node node, DefaultRandom& rng, const std::map<std::string,UParser<O>>& parsers) {
    auto nodeIt = node.begin();
    if (nodeIt != node.end()) {
        auto key = nodeIt->first.as<std::string>();
        if (auto&& parser = parsers.find(key); parser != parsers.end()) {
            if (node.size() != 1) {
                BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Multiple values for recursive map"));
            }
            BOOST_LOG_TRIVIAL(info) << "Found parser for " << key;
            return parser->second(nodeIt->second, rng);
        }
    }
    return std::make_unique<DefaultIfNotFound>(node, rng);
}

UniqueDocumentGenerator pickMapAppendable(YAML::Node node, DefaultRandom& rng) {
    return extractOrConstant<UniqueDocumentGenerator,NormalDocumentGenerator>(node, rng, docParsers);
}

UniqueAppendable pickScalarAppendable(YAML::Node node, DefaultRandom& rng) {
    if (node.Tag() != "!") {
        try {
            return std::make_unique<ConstantIntGenerator>(node, rng);
        } catch (const YAML::BadConversion& e) {
        }
        // TODO: constant double generator
        // TODO: constant boolean generator
        try {
            return std::make_unique<ConstantStringGenerator>(node, rng);
        } catch (const YAML::BadConversion& e) {
        }
    }
    // TODO
    throw InvalidValueGeneratorSyntax("Unknown inside UniqueAppendable pickScalarAppendable(YAML::Node node, DefaultRandom& rng)");
}

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng) {
    BOOST_LOG_TRIVIAL(info) << toString(node) << " size = " << node.size();
    if (node.IsScalar()) {
        return pickScalarAppendable(node, rng);
    }
    if (node.IsNull()) {
        return std::make_unique<NullGenerator>(node, rng);
    }
    if (node.IsSequence()) {
        return std::make_unique<NormalArrayGenerator>(node, rng);
    }
    if (node.IsMap()) {
        return pickMapAppendable(node, rng);
    }
    throw InvalidValueGeneratorSyntax("Unknown insid UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng)");
}

}  // namespace genny
