#include "choose_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <random>

#include "parse_util.hpp"
#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
ChooseGenerator::ChooseGenerator(YAML::Node& node) : ValueGenerator(node) {
    auto choiceNode = node["choices"];
    for (auto choice : choiceNode) {
        choices.push_back(yamlToValue(choice));
    }
}

bsoncxx::array::value ChooseGenerator::generate(threadState& state) {
    uniform_int_distribution<int64_t> distribution(0, choices.size() - 1);
    BOOST_LOG_TRIVIAL(debug) << "Generating string in ChooseGenerator";
    return (choices[distribution(state.rng)]);
}
}
