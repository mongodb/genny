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

#ifndef HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31
#define HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31

#include <gennylib/conventions.hpp>

namespace genny::config {

/**
 * Configuration for a `genny::ExecutionStrategy`.
 */
struct ExecutionStrategyOptions {
    /** Default values for each key */
    struct Defaults {
        static constexpr auto kMaxRetries = size_t{0};
        static constexpr auto kThrowOnFailure = false;
    };

    /** YAML keys to use */
    struct Keys {
        static constexpr auto kMaxRetries = "Retries";
        static constexpr auto kThrowOnFailure = "ThrowOnFailure";
    };

    size_t maxRetries = Defaults::kMaxRetries;
    bool throwOnFailure = Defaults::kThrowOnFailure;
};

}  // namespace genny::config

namespace YAML {

/**
 * Convert to/from `genny::config::ExecutionStrategyOptions` and YAML
 */
template <>
struct convert<genny::config::ExecutionStrategyOptions> {
    using Config = genny::config::ExecutionStrategyOptions;
    using Defaults = typename Config::Defaults;
    using Keys = typename Config::Keys;

    static Node encode(const Config& rhs) {
        Node node;

        node[Keys::kMaxRetries] = rhs.maxRetries;
        node[Keys::kThrowOnFailure] = rhs.throwOnFailure;

        return node;
    }

    static bool decode(const Node& node, Config& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        genny::decodeNodeInto(rhs.maxRetries, node[Keys::kMaxRetries], Defaults::kMaxRetries);
        genny::decodeNodeInto(
            rhs.throwOnFailure, node[Keys::kThrowOnFailure], Defaults::kThrowOnFailure);

        return true;
    }
};

}  // namespace YAML

#endif  // HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31
