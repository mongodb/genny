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

class DocumentGenerator {
public:
    explicit DocumentGenerator(const Node& node, PhaseContext& phaseContext, ActorId id);
    explicit DocumentGenerator(const Node& node, ActorContext& phaseContext, ActorId id);
    explicit DocumentGenerator(const Node& node, DefaultRandom& rng);
    bsoncxx::document::value operator()();
    DocumentGenerator(DocumentGenerator&&) noexcept;
    ~DocumentGenerator();
    class Impl;

private:
    std::unique_ptr<Impl> _impl;
};

}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
