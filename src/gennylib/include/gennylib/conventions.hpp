#ifndef HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
#define HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED

#include <chrono>

#include <yaml-cpp/yaml.h>

#include <gennylib/config/ExecutionStrategyOptions.hpp>
#include <gennylib/config/OperationOptions.hpp>

using namespace genny::config;

namespace YAML {

template <typename T, typename S>
void decodeNodeInto(T& out, const Node& node, const S& fallback) {
    out = node.as<T>(fallback);
}

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
        node["ThrowOnFailure"] = rhs.throwOnFailure;
        return node;
    }

    static bool decode(const Node& node, ExecutionStrategyOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        decodeNodeInto(rhs.maxRetries, node["Retries"], ExecutionStrategyOptions::kDefaultMaxRetries);
        decodeNodeInto(
            rhs.throwOnFailure, node["ThrowOnFailure"], ExecutionStrategyOptions::kDefaultThrowOnFailure);

        return true;
    }
};

template <>
struct convert<OperationOptions> {
    static Node encode(const OperationOptions& rhs) {
        Node node;
        node["OperationPreDelayMS"] = rhs.preDelayMS;
        node["OperationPostDelayMS"] = rhs.postDelayMS;

        if (!rhs.metricsName.empty()) {
            node["OperationMetricsName"] = rhs.metricsName;
        }

        node["OperationIsQuiet"] = rhs.isQuiet;
        return node;
    }

    static bool decode(const Node& node, OperationOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        decodeNodeInto(
            rhs.preDelayMS, node["OperationPreDelayMS"], OperationOptions::kDefaultPreDelayMS);
        decodeNodeInto(
            rhs.postDelayMS, node["OperationPostDelayMS"], OperationOptions::kDefaultPostDelayMS);
        decodeNodeInto(
            rhs.metricsName, node["OperationMetricsName"], OperationOptions::kDefaultMetricsName);
        decodeNodeInto(rhs.isQuiet, node["OperationIsQuiet"], OperationOptions::kDefaultIsQuiet);

        return true;
    }

};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
