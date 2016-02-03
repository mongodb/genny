#include "set_variable.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/view_or_value.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include "workload.hpp"

using bsoncxx::array::view;
using bsoncxx::array::value;
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
        operationNode = operation;
        setType = SetType::OPERATION;
    } else if (node["value"]) {
        myValue = yamlToValue(node["value"]);
        BOOST_LOG_TRIVIAL(trace) << "set_variable has a raw value to use: "
                                 << node["value"].Scalar();
        setType = SetType::VALUE;
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "In set_variable and don't have operation or  a value";
        exit(0);
    }
    BOOST_LOG_TRIVIAL(trace) << "Added op of type set_variable";
}

// Copied from workloadNode and should be consolidated.
// increment a variable and returned the value before the increment
value incrementVar(std::unordered_map<string, bsoncxx::array::value>::iterator var,
                   int increment = 1) {
    auto varview = var->second.view();
    auto elem = varview[0];
    // Make a copy of the value to return
    value returnValue = value(varview);
    // Everything after this is about the update. Should be common with workloadNode and
    // operationDocument
    bsoncxx::builder::stream::array myArray{};
    switch (elem.type()) {
        case bsoncxx::type::k_int64: {
            bsoncxx::types::b_int64 value = elem.get_int64();

            // update the variable
            if (increment) {
                value.value += increment;
                var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            }
            break;
        }
        case bsoncxx::type::k_int32: {
            bsoncxx::types::b_int32 value = elem.get_int32();
            // update the variable
            if (increment) {
                value.value++;
                var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            }
            break;
        }
        case bsoncxx::type::k_double: {
            bsoncxx::types::b_double value = elem.get_double();
            // update the variable
            if (increment) {
                value.value++;
                var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            }
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

value multiplyVar(std::unordered_map<string, bsoncxx::array::value>::iterator var, int64_t factor) {
    auto varview = var->second.view();
    auto elem = varview[0];
    // Make a copy of the value to return
    // Everything after this is about the update. Should be common with workloadNode and
    // operationDocument
    bsoncxx::builder::stream::array myArray{};
    switch (elem.type()) {
        case bsoncxx::type::k_int64: {
            bsoncxx::types::b_int64 value = elem.get_int64();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_int32: {
            bsoncxx::types::b_int32 value = elem.get_int32();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
            break;
        }
        case bsoncxx::type::k_double: {
            bsoncxx::types::b_double value = elem.get_double();
            value.value *= factor;
            return (myArray << value << bsoncxx::builder::stream::finalize);
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

            BOOST_LOG_TRIVIAL(fatal) << "multiplyVar with type unsuported type in list";
            exit(EXIT_FAILURE);
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "multiplyVar with type unsuported type not in list";
            exit(EXIT_FAILURE);
    }
    return (myArray << 0 << bsoncxx::builder::stream::finalize);
}


// Execute the node
void set_variable::execute(mongocxx::client& conn, threadState& state) {
    // pull out the value, either by reading the variable or copying the value
    BOOST_LOG_TRIVIAL(trace) << "In set_variable::execute";
    // Local owned value to ensure value doesn't change after lock is released for workload vars
    optional<bsoncxx::array::value> localValue;
    switch (setType) {
        case SetType::VALUE:
            localValue = bsoncxx::array::value(myValue.view());
            break;
        case SetType::OPERATION:
            // for this one need a value since we're going to manipulate the value and it's
            // going to
            // fall out of scope.
            if (operationNode->IsScalar()) {
                localValue = yamlToValue(*operationNode);
            } else if (operationNode->IsMap()) {
                auto type = (*operationNode)["type"].Scalar();
                if (type == "usevar") {
                    string varname = (*operationNode)["variable"].Scalar();
                    if (state.tvariables.count(varname) > 0) {
                        localValue =
                            bsoncxx::array::value(state.tvariables.find(varname)->second.view());
                    } else if (state.wvariables.count(varname) > 0) {  // in wvariables
                        // Grab lock. Could be kinder hear and wait on condition variable
                        std::lock_guard<std::mutex> lk(state.myWorkload.mut);
                        // FIXME: how do we keep this from changing after we get the view and leave
                        // this section?
                        localValue =
                            bsoncxx::array::value(state.wvariables.find(varname)->second.view());
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) << "In usevar but variable " << varname
                                                 << " doesn't exist";
                        exit(EXIT_FAILURE);
                    }

                } else if (type == "increment") {
                    string varname = (*operationNode)["variable"].Scalar();
                    if (state.tvariables.count(varname) > 0) {
                        auto var = state.tvariables.find(varname);
                        localValue = incrementVar(var);
                    } else if (state.wvariables.count(varname) > 0) {  // in wvariables
                        // Grab lock. Could be kinder hear and wait on condition variable
                        std::lock_guard<std::mutex> lk(state.myWorkload.mut);
                        auto var = state.wvariables.find(varname);
                        localValue = incrementVar(var);
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) << "In increment but variable " << varname
                                                 << " doesn't exist";
                        exit(EXIT_FAILURE);
                    }
                } else if (type == "randomint") {
                    int64_t min = 0;
                    int64_t max = 100;
                    if ((*operationNode)["min"]) {
                        min = (*operationNode)["min"].as<int64_t>();
                    }
                    if ((*operationNode)["max"]) {
                        max = (*operationNode)["max"].as<int64_t>();
                    }
                    uniform_int_distribution<int64_t> distribution(min, max);
                    bsoncxx::builder::stream::array myArray{};
                    localValue = myArray << distribution(state.rng)
                                         << bsoncxx::builder::stream::finalize;
                } else if (type == "randomstring") {
                    int length = 10;
                    // Taking code from bson_template_evaluator to
                    // start. Ideally we could specify the
                    // alphabet in the YAML and default to the
                    // following
                    static const char alphanum[] =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789+/";
                    static const uint alphaNumLength = 64;
                    if ((*operationNode)["length"]) {
                        length = (*operationNode)["length"].as<int>();
                    }
                    std::string str;
                    for (int i = 0; i < length; i++) {
                        uniform_int_distribution<int> distribution(0, alphaNumLength - 1);
                        str.push_back(alphanum[distribution(state.rng)]);
                    }
                    bsoncxx::builder::stream::array myArray{};
                    localValue = myArray << str << bsoncxx::builder::stream::finalize;
                } else if (type == "date") {
                    auto currentTime = time(nullptr);
                    bsoncxx::types::b_date date = bsoncxx::types::b_date(currentTime * 1000);
                    bsoncxx::builder::stream::array myArray{};
                    localValue = myArray << date << bsoncxx::builder::stream::finalize;
                } else if (type == "multiply") {
                    string varname = (*operationNode)["variable"].Scalar();
                    int64_t factor = (*operationNode)["factor"].as<int64_t>();
                    if (state.tvariables.count(varname) > 0) {
                        auto var = state.tvariables.find(varname);
                        localValue = multiplyVar(var, factor);
                    } else if (state.wvariables.count(varname) > 0) {  // in wvariables
                        // Grab lock. Could be kinder hear and wait on condition variable
                        std::lock_guard<std::mutex> lk(state.myWorkload.mut);
                        auto var = state.wvariables.find(varname);
                        localValue = multiplyVar(var, factor);
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) << "In multiply but variable " << varname
                                                 << " doesn't exist";
                        exit(EXIT_FAILURE);
                    }
                } else {
                    BOOST_LOG_TRIVIAL(fatal) << "Operation of type " << type
                                             << " not supported for operations";
                    exit(EXIT_FAILURE);
                }
            } else {
                BOOST_LOG_TRIVIAL(fatal) << "Operation is a list in set_variable";
                exit(EXIT_FAILURE);
            }
            BOOST_LOG_TRIVIAL(error) << "Operation not supported yet in set_variable";
    }
    // Special cases first: Database and collection name
    if (targetVariable.compare("DBName") == 0) {
        // check that the value is a string
        auto val = localValue->view()[0];
        if (val.type() != bsoncxx::type::k_utf8) {
            BOOST_LOG_TRIVIAL(fatal)
                << "Trying to set the database name to something other than a string";
            exit(0);
        }
        state.DBName = val.get_utf8().value.to_string();
    } else if (targetVariable.compare("CollectionName") == 0) {
        // check that the value is a string
        auto val = localValue->view()[0];
        if (val.type() != bsoncxx::type::k_utf8) {
            BOOST_LOG_TRIVIAL(fatal)
                << "Trying to set the collection name to something other than a string";
            exit(0);
        }
        state.CollectionName = val.get_utf8().value.to_string();
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
