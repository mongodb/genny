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

#ifndef HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
#define HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111

#include <string>

#include <gennylib/config/RateLimiterOptions.hpp>
#include <gennylib/conventions.hpp>

namespace genny::config {

struct RunCommandConfig {
    using RateLimit = RateLimiterOptions;
    struct Operation;
};

struct RunCommandConfig::Operation {
    struct Defaults {
        static constexpr auto kMetricsName = "";
        static constexpr auto kIsQuiet = false;
        static constexpr auto kAwaitStepdown = false;
        static constexpr auto kMinPeriod = TimeSpec{};
        static constexpr auto kPreSleep = TimeSpec{};
        static constexpr auto kPostSleep = TimeSpec{};
    };

    struct Keys {
        static constexpr auto kAwaitStepdown = "OperationAwaitStepdown";
        static constexpr auto kMetricsName = "OperationMetricsName";
        static constexpr auto kIsQuiet = "OperationIsQuiet";
        static constexpr auto kMinPeriod = "OperationMinPeriod";
        static constexpr auto kPreSleep = "OperationSleepBefore";
        static constexpr auto kPostSleep = "OperationSleepAfter";
    };

    std::string metricsName = Defaults::kMetricsName;
    bool isQuiet = Defaults::kIsQuiet;
    bool awaitStepdown = Defaults::kAwaitStepdown;
    RateLimit rateLimit =
        RateLimit{Defaults::kMinPeriod, Defaults::kPreSleep, Defaults::kPostSleep};
};

}  // namespace genny::config

namespace YAML {

template <>
struct convert<genny::config::RunCommandConfig::Operation> {
    using Config = genny::config::RunCommandConfig::Operation;
    using Defaults = typename Config::Defaults;
    using Keys = typename Config::Keys;

    static Node encode(const Config& rhs) {
        Node node;

        // If we don't have a MetricsName, this key is null
        node[Keys::kMetricsName] = rhs.metricsName;

        node[Keys::kIsQuiet] = rhs.isQuiet;
        node[Keys::kAwaitStepdown] = rhs.awaitStepdown;

        node[Keys::kMinPeriod] = genny::TimeSpec{rhs.rateLimit.minPeriod};
        node[Keys::kPreSleep] = genny::TimeSpec{rhs.rateLimit.preSleep};
        node[Keys::kPostSleep] = genny::TimeSpec{rhs.rateLimit.postSleep};
        return node;
    }

    static bool decode(const Node& node, Config& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        genny::decodeNodeInto(rhs.metricsName, node[Keys::kMetricsName], Defaults::kMetricsName);
        genny::decodeNodeInto(rhs.isQuiet, node[Keys::kIsQuiet], Defaults::kIsQuiet);
        genny::decodeNodeInto(
            rhs.awaitStepdown, node[Keys::kAwaitStepdown], Defaults::kAwaitStepdown);

        rhs.rateLimit.minPeriod = node[Keys::kMinPeriod].as<genny::TimeSpec>(Defaults::kMinPeriod);
        rhs.rateLimit.preSleep = node[Keys::kPreSleep].as<genny::TimeSpec>(Defaults::kPreSleep);
        rhs.rateLimit.postSleep = node[Keys::kPostSleep].as<genny::TimeSpec>(Defaults::kPostSleep);

        return true;
    }
};

}  // namespace YAML

#endif  // HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
