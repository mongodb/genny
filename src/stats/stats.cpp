#include "stats.hpp"

#include <boost/log/trivial.hpp>
#include <chrono>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/value.hpp>

namespace mwg {
Stats::Stats() {
    reset();
}

void Stats::reset() {
    std::lock_guard<std::mutex> lk(mut);
    count = 0;
    countExceptions = 0;
    minimumMicros = std::chrono::microseconds::max();
    maximumMicros = std::chrono::microseconds::min();
    meanMicros = fpmicros(0);
    secondMomentMicros = fpmicros(0);
}

void Stats::accumulate(const Stats& addStats) {
    BOOST_LOG_TRIVIAL(fatal) << "Stats::accumulate not implemented";
    exit(0);
}

void Stats::recordMicros(std::chrono::microseconds duration) {
    std::lock_guard<std::mutex> lk(mut);
    count++;
    if (duration < minimumMicros)
        minimumMicros = duration;
    if (duration > maximumMicros)
        maximumMicros = duration;

    // This is an implementation of the following algorithm:
    // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm
    // and largely copied from src/mongo/db/pipeline/accumulator_std_dev.cpp
    const auto delta = duration - meanMicros;
    meanMicros += delta / count;
    // note that delta was computed with mean before it was updated.
    secondMomentMicros += delta.count() * (duration - meanMicros);
}

bsoncxx::document::value Stats::getStats(bool withReset) {
    bsoncxx::builder::stream::document document{};
    if (count > 0) {
        document << "count" << getCount();
        if (count > 1) {
            document << "minimumMicros" << getMinimumMicros().count();
            document << "maximumMicros" << getMaximumMicros().count();
            if (count > 2) {
                document << "populationStdDev" << getPopStdDev().count();
            }
        }
        document << "meanMicros" << getMeanMicros().count();
    }
    if (countExceptions > 0) {
        document << "countExceptions" << getCountExceptions();
    }
    if (withReset)
        reset();
    return (document << bsoncxx::builder::stream::finalize);
}
}
