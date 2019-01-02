#ifndef HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
#define HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111

#include <chrono>
#include <string>

namespace genny::config {
struct OperationOptions {
    static constexpr auto kDefaultPreDelayMS = std::chrono::milliseconds{};
    static constexpr auto kDefaultPostDelayMS = std::chrono::milliseconds{};
    static constexpr auto kDefaultMetricsName = "";
    static constexpr auto kDefaultIsQuiet = false;

    std::chrono::milliseconds preDelayMS = kDefaultPreDelayMS;
    std::chrono::milliseconds postDelayMS = kDefaultPostDelayMS;
    std::string metricsName = kDefaultMetricsName;
    bool isQuiet = kDefaultIsQuiet;
};
}  // namespace genny::config

#endif  // HEADER_AB6A8E35_4B60_43C7_BAD7_01B540596111
