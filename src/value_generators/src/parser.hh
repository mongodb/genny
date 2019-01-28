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

#ifndef HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947
#define HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947

#include <string_view>
#include <unordered_set>

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <yaml-cpp/yaml.h>

namespace genny::value_generators::parser {
bool isNumber(std::string);
bool isBool(std::string);
std::string quoteIfNeeded(std::string);
bsoncxx::array::value yamlToValue(YAML::Node);
bsoncxx::document::value parseMap(YAML::Node,
                                  const std::unordered_set<std::string_view>&,
                                  std::string,
                                  std::vector<std::tuple<std::string, std::string, YAML::Node>>&);
bsoncxx::document::value parseMap(YAML::Node);
bsoncxx::array::value parseSequence(YAML::Node node);
bsoncxx::array::value parseSequence(YAML::Node node,
                                    const std::unordered_set<std::string_view>&,
                                    std::string,
                                    std::vector<std::tuple<std::string, std::string, YAML::Node>>&);
}  // namespace genny::value_generators::parser

#endif  // HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947
