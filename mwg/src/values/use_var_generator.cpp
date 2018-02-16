#include "use_var_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
UseVarGenerator::UseVarGenerator(YAML::Node& node) : ValueGenerator(node) {
    // add in error checking
    variableName = node["variable"].Scalar();
}
bsoncxx::array::value UseVarGenerator::generate(threadState& state) {
    if (variableName == "DBName") {
        return (bsoncxx::builder::stream::array() << state.DBName << finalize);
    } else if (variableName.compare("CollectionName") == 0) {
        return (bsoncxx::builder::stream::array() << state.CollectionName << finalize);
    } else if (state.tvariables.count(variableName) > 0) {
        return (bsoncxx::array::value(state.tvariables.find(variableName)->second.view()));
    } else if (state.wvariables.count(variableName) > 0) {  // in wvariables
        // Grab lock. Could be kinder hear and wait on condition variable
        std::lock_guard<std::mutex> lk(state.workloadState->mut);
        return (bsoncxx::array::value(state.wvariables.find(variableName)->second.view()));
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "In usevar but variable " << variableName << " doesn't exist";
        exit(EXIT_FAILURE);
    }
}
}
