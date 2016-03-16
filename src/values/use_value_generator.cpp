#include "use_value_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include "parse_util.hpp"
#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
UseValueGenerator::UseValueGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    if (node.IsScalar()) {
        value = yamlToValue(node);
    } else {
        value = yamlToValue(node["value"]);
    }
}

bsoncxx::array::value UseValueGenerator::generate(threadState& state) {
    // probably should actually return a view or a copy of the value
    return (bsoncxx::array::value(*value));
}
}
