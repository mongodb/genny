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

#ifndef HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
#define HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED

#include <chrono>

#include <yaml-cpp/yaml.h>

namespace genny {

/**
 * This function converts a YAML::Node into a given type and uses a given fallback.
 * It simplifies a common pattern where you have a member variable that needs to be assigned either
 * the value in a node or a fallback value (traditionally, this involves at least a decltype).
 */
template <typename T, typename S>
void decodeNodeInto(T& out, const YAML::Node& node, const S& fallback) {
    out = node.as<T>(fallback);
}

}  // namespace genny

namespace YAML {

using genny::decodeNodeInto;

template <>
struct convert<std::chrono::milliseconds> {
    // For now only accept `Duration: 300` for 300 milliseconds.
    // Future change will parse this as a string e.g. `Duration: 300 milliseconds`.

    static Node encode(const std::chrono::milliseconds& rhs) {
        return Node{rhs.count()};
    }

    static bool decode(const Node& node, std::chrono::milliseconds& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        rhs = std::chrono::milliseconds{node.as<int>()};
        return true;
    }
};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
