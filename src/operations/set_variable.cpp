#include "set_variable.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/view_or_value.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <stdlib.h>

#include "parse_util.hpp"
#include "value_generators.hpp"
#include "workload.hpp"

using bsoncxx::builder::stream::finalize;
using bsoncxx::array::value;
using bsoncxx::array::view;
using view_or_value = bsoncxx::view_or_value<view, value>;

namespace mwg {
set_variable::set_variable(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = set_variable
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Set_Variable constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in set_variable type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "set_variable") {
        BOOST_LOG_TRIVIAL(fatal)
            << "Set_Variable constructor but yaml entry doesn't have type == set_variable";
        exit(EXIT_FAILURE);
    }
    targetVariable = node["target"].Scalar();
    if (auto operation = node["operation"]) {
        valueGenerator = makeUniqueValueGenerator(operation);
    } else if (node["value"]) {
        valueGenerator = makeUniqueValueGenerator(node["value"]);
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "In set_variable and don't have operation or  a value";
        exit(0);
    }
    BOOST_LOG_TRIVIAL(trace) << "Added op of type set_variable";
}

// Execute the node
void set_variable::execute(mongocxx::client& conn, threadState& state) {
    BOOST_LOG_TRIVIAL(trace) << "In set_variable::execute";

    // Local owned value to ensure value doesn't change after lock is
    // released for workload vars. Generating value here.
    optional<bsoncxx::array::value> localValue = valueGenerator->generate(state);

    // Special cases first: Database and collection name
    if (targetVariable.compare("DBName") == 0) {
        // check that the value is a string
        BOOST_LOG_TRIVIAL(trace) << "Setting DBName in set_variable";
        state.DBName = valAsString(*localValue);
    } else if (targetVariable.compare("CollectionName") == 0) {
        // check that the value is a string
        BOOST_LOG_TRIVIAL(trace) << "Setting CollectionName in set_variable";
        state.CollectionName = valAsString(*localValue);
    } else {
        // is targetVariable in tvariables or wvariables?
        // Check tvariables first, then wvariable. If in neither, insert into tvariables.
        BOOST_LOG_TRIVIAL(trace) << "set_variable about to actually set variable";
        if (state.tvariables.count(targetVariable) > 0)
            state.tvariables.find(targetVariable)->second = std::move(*localValue);
        else if (state.wvariables.count(targetVariable) > 0)
            state.wvariables.find(targetVariable)->second = std::move(*localValue);
        else
            state.tvariables.insert({targetVariable, std::move(*localValue)});
        BOOST_LOG_TRIVIAL(trace) << "set_variable after set variable";
    }
}
}
