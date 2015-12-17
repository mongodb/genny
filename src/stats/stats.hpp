#pragma once
#include <mutex>
#include <bsoncxx/document/value.hpp>

namespace mwg {

using fpmicros = std::chrono::duration<double, std::ratio<1, 1000000>>;


class stats {
public:
    stats();
    stats(const stats& other)
        : count(other.count), min(other.min), max(other.max), mean(other.mean), m2(other.m2){};
    stats(stats&&) = default;
    virtual ~stats() = default;
    void reset();

    void accumulate(const stats&);  // add up stats
    // this can be moved into an RAII accumulator
    void record(std::chrono::microseconds);  // record one event of given duration
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

    int64_t getCount() {
        return count;
    }

    bsoncxx::document::value getStats();

private:
    int64_t count;
    std::chrono::microseconds min;
    std::chrono::microseconds max;
    // These next two should be replace with more solid algorithms for accumulation mean and
    // variance
    fpmicros mean;
    fpmicros m2;  // for second moment
    std::mutex mut;
};
}
