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

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <exception>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <value_generators/DefaultRandom.hpp>

namespace genny::value_generators {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidConfigurationException : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};


/*
 * This is the base class for all document generrators. A document generator generates a possibly
 * random bson view that can be used in generating interesting mongodb requests.
 *
 */
class DocumentGenerator {
public:
    virtual ~DocumentGenerator(){};

    /*
     * @param doc
     *  The bson stream builder used to hold state for the view. The view lifetime is tied to that
     * doc.
     *
     * @return
     *  Returns a bson view of the generated document.
     */
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc) {
        return doc.view();
    };
};


/*
 * Factory function to parse a YAML Node and make a document generator of the correct type.
 *
 * @param Node
 *  The YAML node with the configuration for this document generator.
 * @param DefaultRandom
 *  A reference to the random number generator for the owning thread. Internal object may save a
 * reference to this random number generator.
 */
std::unique_ptr<DocumentGenerator> makeDoc(YAML::Node, genny::DefaultRandom&);

}  // namespace genny::value_generators

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
