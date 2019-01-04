#ifndef HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31
#define HEADER_4E5CB3A4_FE6D_49B3_A31A_4237238C5A31

#include <gennylib/conventions.hpp>

namespace genny::config {

struct ExecutionStrategyOptions {
    struct Defaults {
        static constexpr auto kMaxRetries = size_t{0};
        static constexpr auto kThrowOnFailure = false;
    };

    struct Keys {
        static constexpr auto kMaxRetries = "Retries";
        static constexpr auto kThrowOnFailure = "ThrowOnFailure";
    };

    size_t maxRetries = Defaults::kMaxRetries;
    bool throwOnFailure = Defaults::kThrowOnFailure;
};

}  // namespace genny::config

namespace YAML {

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
