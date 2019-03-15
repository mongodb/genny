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

#ifndef HEADER_765C17F3_36B6_4EDC_BA8C_61DD618CCA80_INCLUDED
#define HEADER_765C17F3_36B6_4EDC_BA8C_61DD618CCA80_INCLUDED

#include <exception>

#include <bsoncxx/array/view_or_value.hpp>
#include <bsoncxx/document/view_or_value.hpp>
#include <bsoncxx/types.hpp>

#include <yaml-cpp/yaml.h>

namespace genny::testing {

class InvalidYAMLToBsonException : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

/**
 * @param node yaml map node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a map
 */
bsoncxx::document::view_or_value toDocumentBson(const YAML::Node& node);


/**
 * @param node yaml list node
 * @return bson representation of it.
 * @throws InvalidYAMLToBsonException if node isn't a list (sequence)
 */
bsoncxx::array::view_or_value toArrayBson(const YAML::Node& node);

}  // namespace genny::testing

#endif  // HEADER_765C17F3_36B6_4EDC_BA8C_61DD618CCA80_INCLUDED
