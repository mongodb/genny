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
#include <random>
#include <sstream>
#include <unordered_map>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny {

////////////////////////////////////////////////
// Int Generators
////////////////////////////////////////////////


class IntGenerator::Impl {
    friend class UniformIntGenerator;

public:
    virtual int64_t evaluate() = 0;
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

class StringGenerator::Impl {
    friend class ConstantStringGenerator;

public:
    virtual std::string evaluate() = 0;
    virtual ~Impl() = default;
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
// Appendable
/////////////////////////////////////////////////////


class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
};

using UniqueAppendable = std::unique_ptr<Appendable>;

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng);


/////////////////////////////////////////////////////
// Document Generator
//////////////////////////////////////////////////////

class DocumentGenerator::Impl {
    friend class NormalDocumentGenerator;
    friend class ConstantDocumentGenerator;

public:
    virtual bsoncxx::document::value evaluate() = 0;
    virtual ~Impl() = default;
};

using UniqueDocumentGenerator = std::unique_ptr<DocumentGenerator::Impl>;

UniqueDocumentGenerator pickDocGenerator(YAML::Node node, DefaultRandom &rng);

class NormalDocumentGenerator : public DocumentGenerator::Impl {
    using ElementType = std::pair<std::string, UniqueAppendable>;

public:
    ~NormalDocumentGenerator() = default;
    NormalDocumentGenerator(YAML::Node node, DefaultRandom& rng) {
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


class IntAppendable : public Appendable {
public:
    IntAppendable(YAML::Node node, DefaultRandom& rng)
    : _intGen{pickIntGenerator(node, rng)} {}

    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, _intGen->evaluate()));
    }
    ~IntAppendable() override = default;
private:
    UniqueIntGenerator _intGen;
};

class DocAppendable : public Appendable {
public:
    DocAppendable(YAML::Node node, DefaultRandom& rng)
    : _docGen{pickDocGenerator(node, rng)} {}

    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, _docGen->evaluate()));
    }
    ~DocAppendable() override = default;
private:
    UniqueDocumentGenerator _docGen;
};

class StringAppenable : public Appendable {
public:
    StringAppenable(YAML::Node node, DefaultRandom& rng)
    : _stringGen{pickStringGenerator(node, rng)} {}

    void append(const std::string &key, bsoncxx::builder::basic::document &builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, _stringGen->evaluate()));
    }
    ~StringAppenable() override = default;
private:
    UniqueStringGenerator _stringGen;
};

//////////////////////////////////////
// Pick impls
//////////////////////////////////////

UniqueDocumentGenerator pickDocGenerator(YAML::Node node, DefaultRandom &rng) {

}

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng) {
    if (node.IsScalar()) {
        if (node.Tag() != "!") {
            try {
                return std::make_unique<IntAppendable>(node);
            } catch (const YAML::BadConversion& e) {
            }

            // TODO
//            try {
//                return std::make_unique<BoolAppendable>(Value{node.as<bool>()}, ValueType::Boolean);
//            } catch (const YAML::BadConversion& e) {
//            }
        }
    }
    if (node.IsMap()) {

    }
}

auto foo(YAML::Node node, DefaultRandom& rng) {
    if (!node.IsMap()) {
        throw InvalidValueGeneratorSyntax("Expected mapping type to parse into an expression");
    }

    auto nodeIt = node.begin();
    if (nodeIt == node.end()) {
        return std::make_unique<NormalDocumentGenerator>(node);
    }

    auto key = nodeIt->first;
    auto value = nodeIt->second;

    ++nodeIt;
    if (nodeIt != node.end()) {
        throw InvalidValueGeneratorSyntax(
                "Expected mapping to have a single '^'-prefixed key, but had multiple keys");
    }

    auto name = key.as<std::string>();

    auto parserIt = parserMap.find(name);
    if (parserIt == parserMap.end()) {
        std::stringstream error;
        error << "Unknown expression type '" << name << "'";
        throw InvalidValueGeneratorSyntax(error.str());
    }
    return parserIt->second(value, rng);

    return std::make_unique<NormalDocumentGenerator>(node, rng);

    return std::make_unique<IntAppendable>(node, rng);
}

}  // namespace genny
