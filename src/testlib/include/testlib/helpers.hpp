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

#ifndef HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED
#define HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED

#include <catch2/catch.hpp>

#include <string>

#include <bsoncxx/document/view_or_value.hpp>
#include <bsoncxx/json.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

inline std::string toString(const std::string& str) {
    return str;
}

inline std::string toString(const bsoncxx::document::view_or_value& t) {
    return bsoncxx::to_json(t, bsoncxx::ExtendedJsonMode::k_canonical);
}

inline std::string toString(const YAML::Node& node) {
    YAML::Emitter out;
    out << node;
    return std::string{out.c_str()};
}


}  // namespace genny

#endif  // HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED
