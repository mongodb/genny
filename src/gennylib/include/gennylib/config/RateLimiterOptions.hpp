#ifndef HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
#define HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4

#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::config {
struct RateLimiterOptions {
    std::chrono::milliseconds minPeriod{0};
    std::chrono::milliseconds preSleep{0};
    std::chrono::milliseconds postSleep{0};
};
}  // namespace genny::config

#endif  // HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
