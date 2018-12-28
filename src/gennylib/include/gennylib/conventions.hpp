#ifndef HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
#define HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED

#include <chrono>

#include <yaml-cpp/yaml.h>

#include <gennylib/config/ExecutionStrategyOptions.hpp>

using namespace genny::config;

namespace YAML {

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

template <>
struct convert<ExecutionStrategyOptions> {
    static Node encode(const ExecutionStrategyOptions& rhs) {
        Node node;
        node["Retries"] = rhs.maxRetries;
        return node;
    }

    static bool decode(const Node& node, ExecutionStrategyOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        auto yamlRetries = node["Retries"];
        if (yamlRetries.IsNull()) {
            return false;
        }

        rhs.maxRetries = yamlRetries.as<decltype(rhs.maxRetries)>();
        return true;
    }

};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
