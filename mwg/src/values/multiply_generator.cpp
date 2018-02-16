#include "multiply_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <unordered_map>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::array::value;
using bsoncxx::builder::stream::finalize;

namespace mwg {

MultiplyGenerator::MultiplyGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    for (auto fnode : node["factors"]) {
        factors.push_back(makeUniqueValueGenerator(fnode));
    }
}

double MultiplyGenerator::generateDouble(threadState& state) {
    double result = 1.0;
    for (auto& factor : factors) {
        result *= factor->generateDouble(state);
    }
    return result;
}

int64_t MultiplyGenerator::generateInt(threadState& state) {
    return (static_cast<int64_t>(generateDouble(state)));
}

bsoncxx::array::value MultiplyGenerator::generate(threadState& state) {
    return (bsoncxx::builder::stream::array{} << generateDouble(state)
                                              << bsoncxx::builder::stream::finalize);
}
std::string MultiplyGenerator::generateString(threadState& state) {
    return (std::to_string(generateDouble(state)));
}
}
