#include "set_variable.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include "workload.hpp"

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
    if (node["value"]) {
        if (node["donorVariable"]) {
            BOOST_LOG_TRIVIAL(fatal) << "In set_variable and have a donorVariable and a value";
            exit(0);
        }
        myValue = yamlToValue(node["value"]);
        BOOST_LOG_TRIVIAL(trace) << "set_variable has a raw value to use: "
                                 << node["value"].Scalar();
        useVariable = false;
    } else {
        if (!node["donorVariable"]) {
            BOOST_LOG_TRIVIAL(fatal) << "In set_variable and don't have a donorVariable or a value";
            exit(0);
        }
        donorVariable = node["donorVariable"].Scalar();
        BOOST_LOG_TRIVIAL(trace) << "set_variable has a donor variable to use: " << donorVariable;
        useVariable = true;
    }
    BOOST_LOG_TRIVIAL(trace) << "Added op of type set_variable";
}

// Execute the node
void set_variable::execute(mongocxx::client& conn, threadState& state) {
    // pull out the value, either by reading the variable or copying the value
    BOOST_LOG_TRIVIAL(trace) << "In set_variable::execute";
    if (useVariable) {
        BOOST_LOG_TRIVIAL(trace)
            << "In set_variable::execute and useVariable is true. donorVariable is: "
            << donorVariable;
        // copy the variable value over
        // find it.
        if (state.tvariables.count(donorVariable) > 0) {
            BOOST_LOG_TRIVIAL(trace)
                << "In set_variable::execute and donor variable in tvariables. Setting";
            myValue = state.tvariables.find(donorVariable)->second;
        } else if (state.wvariables.count(donorVariable) > 0) {
            BOOST_LOG_TRIVIAL(trace)
                << "In set_variable::execute and donor variable in wvariables. Setting";
            std::lock_guard<std::mutex> lk(state.myWorkload.mut);
            myValue = state.wvariables.find(donorVariable)->second;
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "in set_variable::execute, and can't find the donor "
                                        "variable in tvariables or wvariables: " << donorVariable;
        }
        BOOST_LOG_TRIVIAL(trace)
            << "In set_variable::execute and after setting myValue from donor variable";
    }

    // Special cases first: Database and collection name
    if (targetVariable.compare("DBName") == 0) {
        // check that the value is a string
        auto val = myValue.view()[0];
        if (val.type() != bsoncxx::type::k_utf8) {
            BOOST_LOG_TRIVIAL(fatal)
                << "Trying to set the database name to something other than a string";
            exit(0);
        }
        state.DBName = val.get_utf8().value.to_string();

    } else if (targetVariable.compare("CollectionName") == 0) {
        // check that the value is a string
        auto val = myValue.view()[0];
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
            state.tvariables.find(targetVariable)->second = myValue;
        else if (state.wvariables.count(targetVariable) > 0)
            state.wvariables.find(targetVariable)->second = myValue;
        else
            state.tvariables.insert({targetVariable, myValue});
        BOOST_LOG_TRIVIAL(trace) << "set_variable after set variable";
    }
}
}
