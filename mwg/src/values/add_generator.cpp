#include "add_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <unordered_map>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::array::value;
using bsoncxx::builder::stream::finalize;

namespace mwg {

AddGenerator::AddGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    for (auto fnode : node["addends"]) {
        addends.push_back(makeUniqueValueGenerator(fnode));
    }
}

double AddGenerator::generateDouble(threadState& state) {
    double result = 0.0;
    for (auto& addend : addends) {
        result += addend->generateDouble(state);
    }
    return result;
}

int64_t AddGenerator::generateInt(threadState& state) {
    return (static_cast<int64_t>(generateDouble(state)));
}

bsoncxx::array::value AddGenerator::generate(threadState& state) {
    return (bsoncxx::builder::stream::array{} << generateDouble(state)
                                              << bsoncxx::builder::stream::finalize);
}
std::string AddGenerator::generateString(threadState& state) {
    return (std::to_string(generateDouble(state)));
}
}
