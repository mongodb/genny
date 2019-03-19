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
// Appendable
/////////////////////////////////////////////////////


class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
};

using UniqueAppendable = std::unique_ptr<Appendable>;

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng);


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

UniqueDocumentGenerator pickDocEvaluator(YAML::Node node, DefaultRandom& rng);

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
    : _impl{pickDocEvaluator(node, rng)} {}

bsoncxx::document::value DocumentGenerator::operator()() {
    return _impl->evaluate();
}

DocumentGenerator::~DocumentGenerator() = default;

UniqueDocumentGenerator pickDocEvaluator(YAML::Node node, DefaultRandom& rng) {
    return std::make_unique<NormalDocumentGenerator>(node, rng);
}


//////////////////////////////////////
// Pick impls
//////////////////////////////////////

UniqueAppendable pickAppendable(YAML::Node node, DefaultRandom& rng) {
    return std::make_unique<IntAppendable>(node, rng);
}

}  // namespace genny
