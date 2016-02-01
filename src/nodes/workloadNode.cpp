#include <bsoncxx/builder/stream/array.hpp>
#include "workloadNode.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include "workload.hpp"

namespace mwg {

workloadNode::workloadNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "workloadNode") {
        BOOST_LOG_TRIVIAL(fatal)
            << "workloadNode constructor but yaml entry doesn't have type == workloadNode";
        exit(EXIT_FAILURE);
    }
    if (auto yamlWorkload = ynode["workload"]) {
        myWorkload = unique_ptr<workload>(new workload(yamlWorkload));
    } else {
        BOOST_LOG_TRIVIAL(fatal)
            << "workloadNode constructor but yaml entry doesn't have a workload entry";
        exit(EXIT_FAILURE);
    }
    if (auto override = ynode["overrides"]) {
        if (override.IsMap()) {
            if (auto dbNameNode = override["database"]) {
                dbName = dbNameNode;
            }
            if (auto collectionNode = override["collection"]) {
                collectionName = collectionNode;
            }
            if (auto workNameNode = override["name"]) {
                workloadName = workNameNode;
            }
            if (auto threadsNode = override["threads"]) {
                numThreads = threadsNode;
            }
            if (auto runLengthNode = override["runLength"]) {
                runLength = runLengthNode;
            }
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Workload node overrides aren't a map";
            exit(EXIT_FAILURE);
        }
    }
}

// Generate a string value for an override.
string overrideString(optional<YAML::Node> node, shared_ptr<threadState> myState) {
    if (node->IsScalar()) {
        return (node->Scalar());
    } else if (node->IsMap()) {
        auto type = (*node)["type"].Scalar();
        if (type == "usevar") {
            string varname = (*node)["variable"].Scalar();
            if (myState->tvariables.count(varname) > 0) {
                auto var = myState->tvariables.find(varname);
                return (var->second.view()[0].get_value().get_utf8().value.to_string());
            } else if (myState->wvariables.count(varname) > 0) {  // in wvariables
                // Grab lock. Could be kinder hear and wait on condition variable
                std::lock_guard<std::mutex> lk(myState->myWorkload.mut);
                auto var = myState->wvariables.find(varname);
                return (var->second.view()[0].get_value().get_utf8().value.to_string());
            } else {
                BOOST_LOG_TRIVIAL(fatal) << "In overrideInt but variable " << varname
                                         << " doesn't exist";
                exit(EXIT_FAILURE);
            }

        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Override of type " << type
                                     << " not supported for string overrides";
            exit(EXIT_FAILURE);
        }

    } else {
        BOOST_LOG_TRIVIAL(fatal) << "Override of dbname by a list";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(fatal) << "Reached end of overridestring. Should have exited before now";
    return ("");
}

// increment a variable and returned the value before the increment
int64_t accessVar(std::unordered_map<string, bsoncxx::array::value>::iterator var,
                  int increment = 0) {
    auto varview = var->second.view();
    auto elem = varview[0];
    int64_t val = 0;
    bsoncxx::builder::stream::array myArray{};
    switch (elem.type()) {
        case bsoncxx::type::k_int64: {
            bsoncxx::types::b_int64 value = elem.get_int64();
            val = value.value;
            // update the variable
            if (increment) {
                value.value += increment;
                var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            }

            break;
        }
        case bsoncxx::type::k_int32: {
            bsoncxx::types::b_int32 value = elem.get_int32();
            val = static_cast<int64_t>(value.value);
            // update the variable
            if (increment) {
                value.value++;
                var->second = (myArray << value << bsoncxx::builder::stream::finalize);
            }
            break;
        }
        case bsoncxx::type::k_double: {
            bsoncxx::types::b_double value = elem.get_double();
            val = static_cast<int64_t>(value.value);
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

            BOOST_LOG_TRIVIAL(fatal) << "accessVar with type unsuported type in list";

            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "accessVar with type unsuported type not in list";
    }
    return (val);
}

// Generate a integer value for an override.
int64_t overrideInt(optional<YAML::Node> node, shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(trace) << "In overrideInt";
    if (node->IsScalar()) {
        return (node->as<int64_t>());
    } else if (node->IsMap()) {
        auto type = (*node)["type"].Scalar();
        int increment = 0;
        if (type == "usevar") {
            increment = 0;
        } else if (type == "increment") {
            increment = 1;
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Override of type " << type
                                     << " not supported for int overrides";
            exit(EXIT_FAILURE);
        }
        string varname = (*node)["variable"].Scalar();
        if (myState->tvariables.count(varname) > 0) {
            auto var = myState->tvariables.find(varname);
            return (accessVar(var, increment));
        } else if (myState->wvariables.count(varname) > 0) {  // in wvariables
            // Grab lock. Could be kinder hear and wait on condition variable
            std::lock_guard<std::mutex> lk(myState->myWorkload.mut);
            auto var = myState->wvariables.find(varname);
            return (accessVar(var, increment));
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "In overrideInt but variable " << varname
                                     << " doesn't exist";
            exit(EXIT_FAILURE);
        }
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "Override of dbname by a list";
        exit(EXIT_FAILURE);
    }
    BOOST_LOG_TRIVIAL(fatal) << "Reached end of overridestring. Should have exited before now";
    return (0);
}

// Execute the node
void workloadNode::execute(shared_ptr<threadState> myState) {
    myWorkload->uri = myState->myWorkload.uri;
    BOOST_LOG_TRIVIAL(debug) << "In workloadNode and executing";
    // set random seed based on current random seed.
    // should it be set in constructor? Is that safe?
    myWorkload->setRandomSeed(myState->rng());
    if (dbName) {
        myWorkload->getState().DBName = overrideString(dbName, myState);
    }
    if (collectionName) {
        myWorkload->getState().CollectionName = overrideString(collectionName, myState);
    }
    if (workloadName) {
        myWorkload->name = overrideString(workloadName, myState);
    }
    if (numThreads) {
        myWorkload->numParallelThreads = overrideInt(numThreads, myState);
    }
    if (runLength) {
        myWorkload->runLength = overrideInt(runLength, myState);
    }
    myWorkload->execute();
}
std::pair<std::string, std::string> workloadNode::generateDotGraph() {
    return (std::pair<std::string, std::string>{name + " -> " + nextName + ";\n",
                                                myWorkload->generateDotGraph()});
}
void workloadNode::logStats() {
    myWorkload->logStats();
}
bsoncxx::document::value workloadNode::getStats(bool withReset) {
    return (myWorkload->getStats(withReset));
}

void workloadNode::stop() {
    stopped = true;
    myWorkload->stop();
}
}
