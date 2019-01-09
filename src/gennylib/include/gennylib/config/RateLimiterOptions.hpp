#ifndef HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
#define HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4

#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::config {
struct RateLimiterOptions {
    struct Keys {
        static constexpr auto kMinPeriod = "MinPeriod";
        static constexpr auto kPreSleep = "SleepBefore";
        static constexpr auto kPostSleep = "SleepAfter";
    };

    struct Defaults {
        static constexpr auto kMinPeriod = std::chrono::milliseconds{0};
        static constexpr auto kPreSleep = std::chrono::milliseconds{0};
        static constexpr auto kPostSleep = std::chrono::milliseconds{0};
    };

    std::chrono::milliseconds minPeriod{0};
    std::chrono::milliseconds preSleep{0};
    std::chrono::milliseconds postSleep{0};
};
}  // namespace genny::config

namespace YAML {

template <>
struct convert<genny::config::RateLimiterOptions> {
    using Config = genny::config::RateLimiterOptions;
    using Defaults = typename Config::Defaults;
    using Keys = typename Config::Keys;

    static Node encode(const Config& rhs) {
        Node node;

        node[Keys::kMinPeriod] = rhs.minPeriod;
        node[Keys::kPreSleep] = rhs.preSleep;
        node[Keys::kPostSleep] = rhs.postSleep;

        return node;
    }

    static bool decode(const Node& node, Config& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        genny::decodeNodeInto(rhs.minPeriod, node[Keys::kMinPeriod], Defaults::kMinPeriod);
        genny::decodeNodeInto(rhs.preSleep, node[Keys::kPreSleep], Defaults::kPreSleep);
        genny::decodeNodeInto(rhs.postSleep, node[Keys::kPostSleep], Defaults::kPostSleep);

        return true;
    }
};

}  // namespace YAML

#endif  // HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
