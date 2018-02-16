#include "fast_random_string_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <random>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;
namespace mwg {


FastRandomStringGenerator::FastRandomStringGenerator(const YAML::Node& node)
    : ValueGenerator(node) {
    if (node["length"]) {
        length = IntOrValue(node["length"]);
    } else {
        length = IntOrValue(10);
    }
}
bsoncxx::array::value FastRandomStringGenerator::generate(threadState& state) {
    std::string str;
    auto thisLength = length.getInt(state);
    str.resize(thisLength);
    auto randomnum = state.rng();
    int bits = 64;
    for (int i = 0; i < thisLength; i++) {
        if (bits < 6) {
            bits = 64;
            randomnum = state.rng();
        }
        str[i] = fastAlphaNum[(randomnum & 0x2f) % fastAlphaNumLength];
        randomnum >>= 6;
        bits -= 6;
    }
    return (bsoncxx::builder::stream::array{} << str << bsoncxx::builder::stream::finalize);
}
}
