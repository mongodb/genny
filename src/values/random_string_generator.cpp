#include "random_string_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <random>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;
namespace mwg {


RandomStringGenerator::RandomStringGenerator(YAML::Node& node) : ValueGenerator(node) {
    if (node["length"]) {
        length = node["length"].as<int64_t>();
    } else {
        length = 10;
    }
    if (node["alphabet"]) {
        alphabet = node["alphabet"].Scalar();
    } else {
        alphabet = alphaNum;
    }
}
bsoncxx::array::value RandomStringGenerator::generate(threadState& state) {
    std::string str;
    auto alphabetLength = alphabet.size();
    uniform_int_distribution<int> distribution(0, alphabetLength - 1);
    for (int i = 0; i < length; i++) {
        str.push_back(alphabet[distribution(state.rng)]);
    }
    return (bsoncxx::builder::stream::array{} << str << bsoncxx::builder::stream::finalize);
}
}
