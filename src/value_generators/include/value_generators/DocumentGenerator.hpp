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
#include <bsoncxx/array/value.hpp>

#include <yaml-cpp/yaml.h>

#include <value_generators/DefaultRandom.hpp>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidValueGeneratorSyntax : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class Int64Generator {
public:
    explicit Int64Generator(YAML::Node node, DefaultRandom& rng);
    int64_t operator()();
    ~Int64Generator();
    class Impl;
private:
    std::unique_ptr<Impl> _impl;
};

class Int32Generator {
public:
    explicit Int32Generator(YAML::Node node, DefaultRandom& rng);
    int32_t operator()();
    ~Int32Generator();
    class Impl;
private:
    std::unique_ptr<Impl> _impl;
};

class StringGenerator {
public:
    explicit StringGenerator(YAML::Node node, DefaultRandom& rng);
    std::string operator()();
    ~StringGenerator();
    class Impl;
private:
    std::unique_ptr<Impl> _impl;
};

class DocumentGenerator {
public:
    // TODO: deprecate in favor of ctor
    static DocumentGenerator create(YAML::Node node, DefaultRandom& rng);
    explicit DocumentGenerator(YAML::Node node, DefaultRandom& rng);
    bsoncxx::document::value operator()();
    DocumentGenerator(DocumentGenerator&&) noexcept;
    ~DocumentGenerator();
    class Impl;
private:
    std::unique_ptr<Impl> _impl;
};

class ArrayGenerator {
public:
    explicit ArrayGenerator(YAML::Node node, DefaultRandom& rng);
    bsoncxx::array::value operator()();
    ~ArrayGenerator();
    class Impl;
private:
    std::unique_ptr<Impl> _impl;
};


}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
