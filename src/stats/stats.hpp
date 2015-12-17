#pragma once
#include <mutex>

namespace mwg {

class stats {
public:
    stats();
    stats(const stats& other)
        : count(other.count), min(other.min), max(other.max), total(other.total){};
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
    std::chrono::microseconds getAvg() {
        if (count > 0)
            return total / count;
        return std::chrono::microseconds(0);
    }
    uint64_t getCount() {
        return count;
    }

private:
    uint64_t count;
    std::chrono::microseconds min;
    std::chrono::microseconds max;
    // These next two should be replace with more solid algorithms for accumulation mean and
    // variance
    std::chrono::microseconds total;
    std::chrono::microseconds total2;  // for second moment
    std::mutex mut;
};
}
