#include "concatenate_generator.hpp"

#include <bsoncxx/builder/stream/array.hpp>
#include "threadState.hpp"

namespace mwg {
ConcatenateGenerator::ConcatenateGenerator(YAML::Node& node) : ValueGenerator(node) {
    for (auto gen : node["parts"]) {
        generators.push_back(makeUniqueValueGenerator(gen));
    }
}
bsoncxx::array::value ConcatenateGenerator::generate(threadState& state) {
    std::string str;
    for (auto& gen : generators) {
        str += gen->generateString(state);
    }
    return (bsoncxx::builder::stream::array{} << str << bsoncxx::builder::stream::finalize);
}
}
