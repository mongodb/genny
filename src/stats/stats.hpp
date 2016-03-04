#pragma once

#include <mutex>
#include <bsoncxx/document/value.hpp>
#include <math.h>

namespace mwg {

using fpmicros = std::chrono::duration<double, std::ratio<1, 1000000>>;


class Stats {
public:
    Stats();
    Stats(const Stats& other)
        : count(other.count),
          countExceptions(other.countExceptions),
          minimumMicros(other.minimumMicros),
          maximumMicros(other.maximumMicros),
          meanMicros(other.meanMicros),
          secondMomentMicros(other.secondMomentMicros){};
    void reset();

    void accumulate(const Stats&);  // add up Stats
    // this can be moved into an RAII accumulator
    void recordMicros(std::chrono::microseconds);  // record one event of given duration
    void recordException();

    std::chrono::microseconds getMinimumMicros() {
        return minimumMicros;
    }
    std::chrono::microseconds getMaximumMicros() {
        return maximumMicros;
    }
    std::chrono::microseconds getMeanMicros() {
        return std::chrono::duration_cast<std::chrono::microseconds>(meanMicros);
    }

    std::chrono::microseconds getSecondMomentMicros() {
        return std::chrono::duration_cast<std::chrono::microseconds>(secondMomentMicros);
    }

    std::chrono::microseconds getPopVariance() {
        if (count > 2)
            return (
                std::chrono::duration_cast<std::chrono::microseconds>(secondMomentMicros / count));
        else
            return std::chrono::microseconds(0);
    }

    std::chrono::microseconds getSampleVariance() {
        if (count > 2)
            return (std::chrono::duration_cast<std::chrono::microseconds>(secondMomentMicros /
                                                                          (count - 1)));
        else
            return std::chrono::microseconds(0);
    }
    std::chrono::microseconds getPopStdDev() {
        if (count > 2)
            // This is ugly. Probably a cleaner way to do it.
            return (std::chrono::duration_cast<std::chrono::microseconds>(
                fpmicros(sqrt((secondMomentMicros / count).count()))));
        else
            return std::chrono::microseconds(0);
    }

    std::chrono::microseconds getSampleStdDev() {
        if (count > 2)
            return (std::chrono::duration_cast<std::chrono::microseconds>(
                fpmicros(sqrt((secondMomentMicros / (count - 1)).count()))));
        else
            return std::chrono::microseconds(0);
    }

    int64_t getCount() {
        return count;
    }

    int64_t getCountExceptions() {
        return countExceptions;
    }

    bsoncxx::document::value getStats(bool withReset);

private:
    int64_t count;
    int64_t countExceptions;
    std::chrono::microseconds minimumMicros;
    std::chrono::microseconds maximumMicros;
    // These next two should be replace with more solid algorithms for accumulation mean and
    // variance
    fpmicros meanMicros;
    fpmicros secondMomentMicros;  // for second moment
    std::mutex mut;
};
}
