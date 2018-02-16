#include "use_result_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
UseResultGenerator::UseResultGenerator(YAML::Node& node) : ValueGenerator(node) {}

bsoncxx::array::value UseResultGenerator::generate(threadState& state) {
    return (bsoncxx::array::value(state.result->view()));
}
}
