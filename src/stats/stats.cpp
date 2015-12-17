#include "stats.hpp"
#include <boost/log/trivial.hpp>
#include <chrono>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/value.hpp>

namespace mwg {
stats::stats() {
    reset();
}

void stats::reset() {
    count = 0;
    min = std::chrono::microseconds::max();
    max = std::chrono::microseconds::min();
    total = std::chrono::microseconds(0);
    total2 = std::chrono::microseconds(0);
}

void stats::accumulate(const stats& addStats) {
    BOOST_LOG_TRIVIAL(fatal) << "stats::accumulate not implemented";
    exit(0);
}

void stats::record(std::chrono::microseconds dur) {
    std::lock_guard<std::mutex> lk(mut);
    count++;
    if (dur < min)
        min = dur;
    if (dur > max)
        max = dur;
    total += dur;
    // total2 += dur * dur;
}

bsoncxx::document::value stats::getStats() {
    bsoncxx::builder::stream::document document{};
    if (count > 0) {
        document << "count" << getCount();
        if (count > 1) {
            document << "min" << getMin().count();
            document << "max" << getMax().count();
        }
        document << "average" << getAvg().count();
    }
    return (document << bsoncxx::builder::stream::finalize);
}
}
