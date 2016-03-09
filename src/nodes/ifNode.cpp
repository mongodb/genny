#include "ifNode.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <random>
#include <stdio.h>
#include <stdlib.h>

#include "parse_util.hpp"
#include "workload.hpp"

using view_or_value = bsoncxx::view_or_value<bsoncxx::array::view, bsoncxx::array::value>;

namespace mwg {
ifNode::ifNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "ifNode") {
        BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but yaml entry doesn't have type == "
                                    "ifNode";
        exit(EXIT_FAILURE);
    }

    ifNodeName = ynode["ifNode"].Scalar();
    if (!ynode["elseNode"].IsScalar()) {
        BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but elseNode isn't a scalar";
        exit(EXIT_FAILURE);
    }
    elseNodeName = ynode["elseNode"].Scalar();
    // default to comparing against result
    comparisonVariable = "result";
    if (ynode["comparison"] && ynode["comparison"].IsMap()) {
        auto compNode = ynode["comparison"];
        if (!compNode["value"]) {
            BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but no comparison value";
            exit(EXIT_FAILURE);
        }
        compareValue = yamlToValue(compNode["value"]);
        if (compNode["variable"]) {
            comparisonVariable = compNode["variable"].Scalar();
        }
        if (auto test = compNode["test"]) {
            if (!test.IsScalar()) {
                BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but comparison.test exist but "
                                            "isn't a scalar";
                exit(EXIT_FAILURE);
            }
            auto testType = test.Scalar();
            BOOST_LOG_TRIVIAL(trace) << "IfNode with comparison type " << testType;
            if (testType == "equals")
                comparisonTest = comparison::EQUALS;
            else if (testType == "greater")
                comparisonTest = comparison::GREATER_THAN;
            else if (testType == "less")
                comparisonTest = comparison::LESS_THAN;
            else if (testType == "greater_or_equal")
                comparisonTest = comparison::GREATER_THAN_EQUAL;
            else if (testType == "less_or_equal")
                comparisonTest = comparison::LESS_THAN_EQUAL;
            else {
                BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but invalid comparison.test "
                                         << testType;

                exit(EXIT_FAILURE);
            }

        } else {
            BOOST_LOG_TRIVIAL(trace) << "No test type set. Using default";
            comparisonTest = comparison::EQUALS;
        }
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "IfNode constructor but comparison field isn't a map";
        exit(EXIT_FAILURE);
    }

    nextName = ifNodeName;
    BOOST_LOG_TRIVIAL(debug) << "Setting nextName to first entry. NextName: " << nextName;
}

void ifNode::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                         vector<shared_ptr<node>>& vectornodesin) {
    BOOST_LOG_TRIVIAL(debug) << "Setting next nodes in IfNode";
    iffNode = nodes[ifNodeName];
    elseNode = nodes[elseNodeName];
    nextNode = iffNode;
    BOOST_LOG_TRIVIAL(debug) << "Set next nodes in ifNode";
}

// Execute the node
void ifNode::executeNode(shared_ptr<threadState> myState) {
    chrono::high_resolution_clock::time_point start, stop;
    start = chrono::high_resolution_clock::now();
    BOOST_LOG_TRIVIAL(debug) << "ifNode.execute.";
    if (stopped || myState->stopped)  // short circuit and return if stopped flag set
        return;
    shared_ptr<node> me = myState->currentNode;
    // this comparison should be more general.
    // switch through tests to compute flag
    bool conState = false;
    view_or_value resultView;
    if (comparisonVariable == "result") {
        resultView = myState->result->view();
    } else if (myState->tvariables.count(comparisonVariable) > 0) {
        resultView = myState->tvariables.find(comparisonVariable)->second.view();
    } else if (myState->wvariables.count(comparisonVariable) > 0) {
        std::lock_guard<std::mutex> lk(myState->workloadState.mut);
        // make a copy so we can drop the lock
        resultView =
            bsoncxx::array::value(myState->wvariables.find(comparisonVariable)->second.view());
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "In ifNode but variable " << comparisonVariable
                                 << " doesn't exist";
        exit(EXIT_FAILURE);
    }
    auto compareView = compareValue->view();
    switch (comparisonTest) {
        case comparison::EQUALS:
            conState = (bsoncxx::to_json(resultView.view()) == bsoncxx::to_json(compareView));
            BOOST_LOG_TRIVIAL(trace) << "In EQUALS than comparison with result "
                                     << bsoncxx::to_json(resultView.view())
                                     << " and compare value     " << bsoncxx::to_json(compareView)
                                     << " conState=" << conState;
            break;
        case comparison::GREATER_THAN:
            // Assuming integer time for comparison ops. Should be more general
            // BOOST_LOG_TRIVIAL(trace) << "In Greater than comparison with result "
            //                          << myState->result->get_int64().value << " and compare
            //                          value
            //                          "
            //                          << compareValue->get_int64().value;

            if (resultView.view()[0].type() != compareView[0].type())
                BOOST_LOG_TRIVIAL(error) << "In Greater than but different types";
            else {
                switch (resultView.view()[0].type()) {
                    case bsoncxx::type::k_date:
                    case bsoncxx::type::k_int64:
                        conState = (resultView.view()[0].get_int64().value >
                                    compareView[0].get_int64().value);
                        break;
                    case bsoncxx::type::k_int32:
                        conState = (resultView.view()[0].get_int32().value >
                                    compareView[0].get_int32().value);
                        break;
                    case bsoncxx::type::k_double:
                        conState = (resultView.view()[0].get_double().value >
                                    compareView[0].get_double().value);
                        break;
                    default:
                        BOOST_LOG_TRIVIAL(error)
                            << "In Greater than but type is not int32,int64, date, or double";
                }
            }
            break;
        case comparison::LESS_THAN:
            // BOOST_LOG_TRIVIAL(trace) << "In less than comparison with result "
            //                          << myState->result->get_int64().value << " and compare
            //                          value
            //                          "
            //                          << compareValue->get_int64().value;
            if (resultView.view()[0].type() != compareView[0].type())
                BOOST_LOG_TRIVIAL(error) << "In Greater than but different types";
            else {
                switch (resultView.view()[0].type()) {
                    case bsoncxx::type::k_date:
                    case bsoncxx::type::k_int64:
                        conState = (resultView.view()[0].get_int64().value <
                                    compareView[0].get_int64().value);
                        break;
                    case bsoncxx::type::k_int32:
                        conState = (resultView.view()[0].get_int32().value <
                                    compareView[0].get_int32().value);
                        break;
                    case bsoncxx::type::k_double:
                        conState = (resultView.view()[0].get_double().value <
                                    compareView[0].get_double().value);
                        break;
                    default:
                        BOOST_LOG_TRIVIAL(error)
                            << "In Greater than but type is not int32,int64, date, or double";
                }
            }
            break;
        case comparison::GREATER_THAN_EQUAL:
            // BOOST_LOG_TRIVIAL(trace) << "In Greater than equal comparison with result "
            //                          << myState->result->get_int64().value << " and compare
            //                          value
            //                          "
            //                          << compareValue->get_int64().value;
            if (resultView.view()[0].type() != compareView[0].type())
                BOOST_LOG_TRIVIAL(error) << "In Greater than but different types";
            else {
                switch (resultView.view()[0].type()) {
                    case bsoncxx::type::k_date:
                    case bsoncxx::type::k_int64:
                        conState = (resultView.view()[0].get_int64().value >=
                                    compareView[0].get_int64().value);
                        break;
                    case bsoncxx::type::k_int32:
                        conState = (resultView.view()[0].get_int32().value >=
                                    compareView[0].get_int32().value);
                        break;
                    case bsoncxx::type::k_double:
                        conState = (resultView.view()[0].get_double().value >=
                                    compareView[0].get_double().value);
                        break;
                    default:
                        BOOST_LOG_TRIVIAL(error)
                            << "In Greater than but type is not int32,int64, date, or double";
                }
            }
            break;
        case comparison::LESS_THAN_EQUAL:
            // BOOST_LOG_TRIVIAL(trace) << "In less than equal comparison with result "
            //                          << myState->result->get_int64().value << " and compare
            //                          value
            //                          "
            //                          << compareValue->get_int64().value;
            if (resultView.view()[0].type() != compareView[0].type())
                BOOST_LOG_TRIVIAL(error) << "In Greater than but different types";
            else {
                switch (resultView.view()[0].type()) {
                    case bsoncxx::type::k_date:
                    case bsoncxx::type::k_int64:
                        conState = (resultView.view()[0].get_int64().value <=
                                    compareView[0].get_int64().value);
                        break;
                    case bsoncxx::type::k_int32:
                        conState = (resultView.view()[0].get_int32().value <=
                                    compareView[0].get_int32().value);
                        break;
                    case bsoncxx::type::k_double:
                        conState = (resultView.view()[0].get_double().value <=
                                    compareView[0].get_double().value);
                        break;
                    default:
                        BOOST_LOG_TRIVIAL(error)
                            << "In Greater than but type is not int32,int64, date, or double";
                }
            }
            break;
        default:
            BOOST_LOG_TRIVIAL(fatal) << "ifNode executeNode and unhandled comparisonTest value";
    }

    if (conState)
        myState->currentNode = iffNode;
    else
        myState->currentNode = elseNode;
    stop = chrono::high_resolution_clock::now();
    myStats.recordMicros(std::chrono::duration_cast<chrono::microseconds>(stop - start));
    if (!text.empty()) {
        BOOST_LOG_TRIVIAL(info) << text;
    }
    return;
}
std::pair<std::string, std::string> ifNode::generateDotGraph() {
    string graph;
    graph += name + " -> " + ifNodeName + ";\n";
    graph += name + " -> " + elseNodeName + ";\n";
    return (std::pair<std::string, std::string>{graph, ""});
}
}
