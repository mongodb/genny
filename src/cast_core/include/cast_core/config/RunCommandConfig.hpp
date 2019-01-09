#ifndef HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
#define HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111

#include <string>

#include <gennylib/conventions.hpp>
#include <gennylib/config/RateLimiterOptions.hpp>

namespace genny::config {

struct RunCommandOperationOptions {
    struct Defaults {
        static constexpr auto kMetricsName = "";
        static constexpr auto kIsQuiet = false;
        static constexpr auto kRateLimit = RateLimiterOptions{};
    };

    struct Keys {
        static constexpr auto kMetricsName = "OperationMetricsName";
        static constexpr auto kIsQuiet = "OperationIsQuiet";
        static constexpr auto kRateLimit = "OperationRateLimit";
    };

    std::string metricsName = Defaults::kMetricsName;
    bool isQuiet = Defaults::kIsQuiet;
    RateLimiterOptions rateLimit = Defaults::kRateLimit;
};

struct RunCommandConfig{
    using Operation = RunCommandOperationOptions;
};

}  // namespace genny::config

namespace YAML {

template <>
struct convert<genny::config::RunCommandOperationOptions> {
    using Config = genny::config::RunCommandOperationOptions;
    using Defaults = typename Config::Defaults;
    using Keys = typename Config::Keys;

    static Node encode(const Config& rhs) {
        Node node;

        // If we don't have a MetricsName, this key is null
        node[Keys::kMetricsName] = rhs.metricsName;
        node[Keys::kIsQuiet] = rhs.isQuiet;
        node[Keys::kRateLimit] = rhs.rateLimit;

        return node;
    }

    static bool decode(const Node& node, Config& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        genny::decodeNodeInto(rhs.metricsName, node[Keys::kMetricsName], Defaults::kMetricsName);
        genny::decodeNodeInto(rhs.isQuiet, node[Keys::kIsQuiet], Defaults::kIsQuiet);
        genny::decodeNodeInto(rhs.rateLimit, node[Keys::kRateLimit], Defaults::kRateLimit);

        return true;
    }
};

}  // namespace YAML

#endif  // HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
