#include <stdio.h>
#include <stdlib.h>
#include "random_choice.hpp"
#include "parse_util.hpp"
#include <random>
#include <boost/log/trivial.hpp>

namespace mwg {
random_choice::random_choice(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = random_choice
    if (!ynode) {
        BOOST_LOG_TRIVIAL(fatal) << "Random_Choice constructor and !ynode";
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in random_choice type initializer";
        exit(EXIT_FAILURE);
    }
    if (ynode["type"].Scalar() != "random_choice") {
        BOOST_LOG_TRIVIAL(fatal) << "Random_Choice constructor but yaml entry doesn't have type == "
                                    "random_choice";
        exit(EXIT_FAILURE);
    }
    name = ynode["name"].Scalar();
    if (!ynode["next"].IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Random_Choice constructor but next isn't a map";
        exit(EXIT_FAILURE);
    }

    total = 0;  // figure out how much the next state rates add up to
    BOOST_LOG_TRIVIAL(debug) << "In random choice constructor. About to go through next";
    for (auto entry : ynode["next"]) {
        BOOST_LOG_TRIVIAL(debug) << "Next state: " << entry.first.Scalar()
                                 << " probability: " << entry.second.Scalar();
        vectornodestring.push_back(
            pair<string, double>(entry.first.Scalar(), entry.second.as<double>()));
        total += entry.second.as<double>();
    }
    nextName = vectornodestring[0].first;
    BOOST_LOG_TRIVIAL(debug) << "Setting nextName to first entry. NextName: " << nextName;
}

void random_choice::setNextNode(unordered_map<string, shared_ptr<node>>& nodes,
                                vector<shared_ptr<node>>& vectornodesin) {
    double partial = 0;  // put in distribution
    BOOST_LOG_TRIVIAL(debug) << "Setting next nodes in random choice";
    for (auto nextstate : vectornodestring) {
        partial += (nextstate.second / total);
        vectornodes.push_back(pair<shared_ptr<node>, double>(nodes[nextstate.first], partial));
    }
    BOOST_LOG_TRIVIAL(debug) << "Set next nodes in random choice";
}

// Execute the node
void random_choice::executeNode(shared_ptr<threadState> myState) {
    // pick a random number, and pick the next state based on it.
    BOOST_LOG_TRIVIAL(debug) << "random_choice.execute.";
    double random_number = 0.6;  // Using 0.6 until wiring through the random number generator
    uniform_real_distribution<double> distribution(0.0, 1.0);
    random_number = distribution(myState->rng);
    BOOST_LOG_TRIVIAL(debug) << "random_choice.execute. Random_number is " << random_number;
    for (auto nextstate : vectornodes) {
        if (nextstate.second > random_number) {
            shared_ptr<node> me = myState->currentNode;
            // execute this one
            if (stopped || myState->stopped) {  // short circuit and return if stopped flag set
                BOOST_LOG_TRIVIAL(debug) << "Stopped set";
                myState->currentNode = nullptr;
                return;
            }
            BOOST_LOG_TRIVIAL(debug) << " Next state is " << nextstate.first->name;
            myState->currentNode = nextstate.first;
            return;
        }
    }
}
std::pair<std::string, std::string> random_choice::generateDotGraph() {
    string graph;
    for (auto nextNodeN : vectornodestring) {
        graph += name + " -> " + nextNodeN.first + "[label=\"" + std::to_string(nextNodeN.second) +
            "\"];\n";
    }
    return (std::pair<std::string, std::string>{graph, ""});
}
}
