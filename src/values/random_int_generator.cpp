#include "random_int_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <random>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
RandomIntGenerator::RandomIntGenerator(YAML::Node& node) : ValueGenerator(node), min(0), max(100) {
    if (node["min"]) {
        min = node["min"].as<int64_t>();
    }
    if (node["max"]) {
        max = node["max"].as<int64_t>();
    }
    BOOST_LOG_TRIVIAL(debug) << "RandomIntGenerator constructor. min=" << min << " max=" << max;
}

int64_t RandomIntGenerator::generateInt(threadState& state) {
    uniform_int_distribution<int64_t> distribution(min, max);
    return (distribution(state.rng));
}
std::string RandomIntGenerator::generateString(threadState& state) {
    return (std::to_string(generateInt(state)));
}
bsoncxx::array::value RandomIntGenerator::generate(threadState& state) {
    return (bsoncxx::builder::stream::array{} << generateInt(state)
                                              << bsoncxx::builder::stream::finalize);
}
}
