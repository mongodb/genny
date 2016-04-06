// Copyright 2016 MongoDB

#include "increment_generator.hpp"

#include <limits>
#include <string>
#include <unordered_map>


#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
// Copied from workloadNode and should be consolidated.
// increment a variable and returned the value before the increment
bsoncxx::array::value incrementVar(std::unordered_map<string, bsoncxx::array::value>::iterator var,
                                   int64_t increment = 1,
                                   int64_t minimum = std::numeric_limits<int64_t>::min(),
                                   int64_t maximum = std::numeric_limits<int64_t>::max()) {
    BOOST_LOG_TRIVIAL(trace) << "incrementVar and minimum=" << minimum << ", maximum=" << maximum
                             << ", and increment=" << increment;
    auto varview = var->second.view();
    auto elem = varview[0];
    // Make a copy of the value to return
    bsoncxx::array::value returnValue = bsoncxx::array::value(varview);
    // Everything after this is about the update. Should be common with workloadNode and
    // operationDocument
    bsoncxx::builder::stream::array myArray{};
    switch (elem.type()) {
        case bsoncxx::type::k_int64: {
            bsoncxx::types::b_int64 value = elem.get_int64();

            // update the variable
            value.value += increment;
            BOOST_LOG_TRIVIAL(trace) << "increment=" << increment << " and new value is "
                                     << value.value;
            if (value.value > maximum)
                value.value = value.value - maximum + minimum;
            var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_int32: {
            bsoncxx::types::b_int32 value = elem.get_int32();
            // update the variable
            value.value += increment;
            BOOST_LOG_TRIVIAL(trace) << "increment=" << increment << " and new value is "
                                     << value.value;
            if (value.value > maximum)
                value.value = value.value - maximum + minimum;
            var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_double: {
            bsoncxx::types::b_double value = elem.get_double();
            // update the variable
            value.value += increment;
            BOOST_LOG_TRIVIAL(trace) << "increment=" << increment << " and new value is "
                                     << value.value;
            if (value.value > maximum)
                value.value = value.value - maximum + minimum;
            var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_utf8:
        case bsoncxx::type::k_document:
        case bsoncxx::type::k_array:
        case bsoncxx::type::k_binary:
        case bsoncxx::type::k_undefined:
        case bsoncxx::type::k_oid:
        case bsoncxx::type::k_bool:
        case bsoncxx::type::k_date:
        case bsoncxx::type::k_null:
        case bsoncxx::type::k_regex:
        case bsoncxx::type::k_dbpointer:
        case bsoncxx::type::k_code:
        case bsoncxx::type::k_symbol:
        case bsoncxx::type::k_timestamp:

            BOOST_LOG_TRIVIAL(fatal) << "incrementVar with type unsuported type in list";

            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "accessVar with type unsuported type not in list";
    }
    return (returnValue);
}


IncrementGenerator::IncrementGenerator(YAML::Node& node)
    : ValueGenerator(node),
      minimum(std::numeric_limits<int64_t>::min()),
      maximum(std::numeric_limits<int64_t>::max()),
      increment(1) {
    // add in error checking
    BOOST_LOG_TRIVIAL(trace) << "IncrementGenerator constructor";
    variableName = node["variable"].Scalar();
    if (auto minimum_node = node["minimum"])
        minimum = IntOrValue(minimum_node);
    if (auto maximum_node = node["maximum"])
        maximum = IntOrValue(maximum_node);
    if (auto increment_node = node["increment"])
        increment = IntOrValue(increment_node);
}

bsoncxx::array::value IncrementGenerator::generate(threadState& state) {
    if (state.tvariables.count(variableName) > 0) {
        return (incrementVar(state.tvariables.find(variableName),
                             increment.getInt(state),
                             minimum.getInt(state),
                             maximum.getInt(state)));
    } else if (state.wvariables.count(variableName) > 0) {  // in wvariables
        // Grab lock. Could be kinder hear and wait on condition variable
        std::lock_guard<std::mutex> lk(state.workloadState.mut);
        return (incrementVar(state.wvariables.find(variableName),
                             increment.getInt(state),
                             minimum.getInt(state),
                             maximum.getInt(state)));
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "In usevar but variable " << variableName << " doesn't exist";
        exit(EXIT_FAILURE);
    }
}
}  // namespace mwg
