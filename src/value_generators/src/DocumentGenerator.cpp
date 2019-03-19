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
    friend class UniformIntGenerator;

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

UniqueIntGenerator pickIntGenerator(YAML::Node node, DefaultRandom &rng);

class UniformIntGenerator : public IntGenerator::Impl {
public:
    UniformIntGenerator(YAML::Node node, DefaultRandom& rng)
        : _minGen{pickIntGenerator(node["min"], rng)},
          _maxGen{pickIntGenerator(node["max"], rng)},
          _rng{rng} {}
    ~UniformIntGenerator() override = default;

    int64_t evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        auto distribution = std::uniform_int_distribution<int64_t>{min, max};
        return distribution(_rng);
    }

private:
    UniqueIntGenerator _minGen;
    UniqueIntGenerator _maxGen;
    DefaultRandom& _rng;
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

// TODO: move to appendables section
UniqueIntGenerator pickIntGenerator(YAML::Node node, DefaultRandom &rng) {
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
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }
}

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

UniqueDocumentGenerator pickDocGenerator(YAML::Node node, DefaultRandom &rng);

class NormalDocumentGenerator : public DocumentGenerator::Impl {
    using ElementType = std::pair<std::string, UniqueAppendable>;

public:
    ~NormalDocumentGenerator() = default;
    NormalDocumentGenerator(YAML::Node node, DefaultRandom& rng) {
        // will always be a map but may be e.g. {^RandomInt:} or {a:7}
        if (!node.IsMap()) {
            throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an object");
        }

        auto elements = std::vector<ElementType>{};
        for (auto&& entry : node) {
            if (entry.first.as<std::string>()[0] == '^') {
                throw InvalidValueGeneratorSyntax(
                        "'^'-prefix keys are reserved for expressions, but attempted to parse as an"
                        " object");
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
        : _impl{pickDocGenerator(node, rng)} {}

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

UniqueDocumentGenerator pickDocGenerator(YAML::Node node, DefaultRandom &rng) {
    return std::make_unique<NormalDocumentGenerator>(node, rng);
}


UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng) {
    BOOST_LOG_TRIVIAL(info) << toString(node);
    if (node.IsScalar()) {
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
    }

    if (node.IsNull()) {
        return std::make_unique<NullGenerator>(node, rng);
    }

    // TODO: array generator
    if (node.IsMap()) {
        auto nodeIt = node.begin();
        if (nodeIt == node.end()) {
            return std::make_unique<NormalDocumentGenerator>(node, rng);
        }

        auto key = nodeIt->first;
        auto value = nodeIt->second;

        ++nodeIt;
        if (nodeIt != node.end()) {
            return std::make_unique<NormalDocumentGenerator>(node, rng);
        }

        auto name = key.as<std::string>();

//        auto parserIt = parserMap.find(name);
//        if (parserIt == parserMap.end()) {
//            std::stringstream error;
//            error << "Unknown expression type '" << name << "'";
//            throw InvalidValueGeneratorSyntax(error.str());
//        }
//        return parserIt->second(value, rng);
    }
    throw InvalidValueGeneratorSyntax("Unknown");
}

}  // namespace genny
