#include "date_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <stdlib.h>

#include "parse_util.hpp"
#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
DateGenerator::DateGenerator(YAML::Node& node) : ValueGenerator(node) {}

bsoncxx::array::value DateGenerator::generate(threadState& state) {
    auto currentTime = time(nullptr);
    bsoncxx::types::b_date date = bsoncxx::types::b_date(currentTime * 1000);
    bsoncxx::builder::stream::array myArray{};
    return (myArray << date << bsoncxx::builder::stream::finalize);
}
}
