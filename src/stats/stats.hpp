#pragma once
#include <mutex>
#include <bsoncxx/document/value.hpp>
#include <math.h>

namespace mwg {

using fpmicros = std::chrono::duration<double, std::ratio<1, 1000000>>;


class stats {
public:
    stats();
    stats(const stats& other)
        : count(other.count),
          countExceptions(other.countExceptions),
          min(other.min),
          max(other.max),
          mean(other.mean),
          m2(other.m2){};
    stats(stats&&) = default;
    virtual ~stats() = default;
    void reset();

    void accumulate(const stats&);  // add up stats
    // this can be moved into an RAII accumulator
    void record(std::chrono::microseconds);  // record one event of given duration
    void recordException() {
        countExceptions++;
    }
    std::chrono::microseconds getMin() {
        return min;
    }
    std::chrono::microseconds getMax() {
        return max;
    }
    std::chrono::microseconds getMean() {
        return std::chrono::duration_cast<std::chrono::microseconds>(mean);
    }

    std::chrono::microseconds getM2() {
        return std::chrono::duration_cast<std::chrono::microseconds>(m2);
    }

    std::chrono::microseconds getPopVariance() {
        if (count > 2)
            return (std::chrono::duration_cast<std::chrono::microseconds>(m2 / count));
        else
            return std::chrono::microseconds(0);
    }

    std::chrono::microseconds getSampleVariance() {
        if (count > 2)
            return (std::chrono::duration_cast<std::chrono::microseconds>(m2 / (count - 1)));
        else
            return std::chrono::microseconds(0);
    }
    std::chrono::microseconds getPopStdDev() {
        if (count > 2)
            // This is ugly. Probably a cleaner way to do it.
            return (std::chrono::duration_cast<std::chrono::microseconds>(
                fpmicros(sqrt((m2 / count).count()))));
        else
            return std::chrono::microseconds(0);
    }

    std::chrono::microseconds getSampleStdDev() {
        if (count > 2)
            return (std::chrono::duration_cast<std::chrono::microseconds>(
                fpmicros(sqrt((m2 / (count - 1)).count()))));
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
    std::chrono::microseconds min;
    std::chrono::microseconds max;
    // These next two should be replace with more solid algorithms for accumulation mean and
    // variance
    fpmicros mean;
    fpmicros m2;  // for second moment
    std::mutex mut;
};
}
