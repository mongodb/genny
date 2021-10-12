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

#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <exception>
#include <memory>
#include <string>

#include <boost/date_time.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/document/value.hpp>

#include <gennylib/Node.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DefaultRandom.hpp>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidValueGeneratorSyntax : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

/**
 * Throw this to indicate a bad date format is received.
 */
class InvalidDateFormat : public InvalidValueGeneratorSyntax {
public:
    using InvalidValueGeneratorSyntax::InvalidValueGeneratorSyntax;
};

/**
 * Throw this to indicate no parser can be found for a generator.
 */
class UnknownParserException : public InvalidValueGeneratorSyntax {
public:
    using InvalidValueGeneratorSyntax::InvalidValueGeneratorSyntax;
};

struct GeneratorArgs {
    DefaultRandom& rng;
    ActorId actorId;
};


// TODO: This code until the next todo are exposed to get intGenerator and doubleGenerator to work.
// We otherwise do not want to expose them.
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
    ~Generator() override = default;
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
const static boost::posix_time::ptime epoch{boost::gregorian::date(1970, 1, 1)};

// TODO: The code from the previous todo are exposed to get intGenerator and doubleGenerator to
// work. We otherwise do not want to expose them.

UniqueGenerator<int64_t> intGenerator(const Node& node, GeneratorArgs generatorArgs);
UniqueGenerator<double> doubleGenerator(const Node& node, GeneratorArgs generatorArgs);

class DocumentGenerator {
public:
    explicit DocumentGenerator(const Node& node, PhaseContext& phaseContext, ActorId id);
    explicit DocumentGenerator(const Node& node, ActorContext& phaseContext, ActorId id);
    explicit DocumentGenerator(const Node& node, GeneratorArgs generatorArgs);
    /**
     * @return a document according to the template given by the node in the constructor.
     */
    bsoncxx::document::value operator()();
    /**
     * Same as `operator()()` but this syntax allows you to use in a ptr-like context more
     * easily e.g.
     *
     * ```c++
     * auto docGen = context["Document"].maybe<DocumentGenerator>(context, id);
     * ...
     * if(docGen) {
     *     // use operator()()
     *     auto doc = (*filter)();
     *     // or evaluate()
     *     auto doc = filter->evaluate();
     * }
     * ```
     * @return
     */
    bsoncxx::document::value evaluate();
    DocumentGenerator(DocumentGenerator&&) noexcept;
    ~DocumentGenerator();
    class Impl;

private:
    std::unique_ptr<Impl> _impl;
};

}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
