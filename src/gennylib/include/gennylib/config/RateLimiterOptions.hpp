#ifndef HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
#define HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4

#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::config {
struct RateLimiterOptions {
    Duration minPeriod = Duration::zero();
    Duration preSleep = Duration::zero();
    Duration postSleep = Duration::zero();
};
}  // namespace genny::config

#endif  // HEADER_9E3B061B_2B8A_4F4F_BCFE_F9B23B0A1FA4
