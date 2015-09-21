#include <stdlib.h>
#include <iostream>
#include <thread>
#include <boost/log/trivial.hpp>

#include <mongocxx/instance.hpp>

#include "workload.hpp"
#include "node.hpp"
#include "opNode.hpp"
#include "random_choice.hpp"
#include "sleep.hpp"
#include "finish_node.hpp"
#include "forN.hpp"

namespace mwg {
workload::workload(YAML::Node& inputNodes) : stopped(false) {
    if (!inputNodes) {
        BOOST_LOG_TRIVIAL(fatal) << "Workload constructor and !nodes";
        exit(EXIT_FAILURE);
    }
    unordered_map<string, shared_ptr<node>> nodes;
    YAML::Node yamlNodes;
    if (inputNodes.IsMap()) {
        // read out things like the seed
        yamlNodes = inputNodes["nodes"];
        if (!yamlNodes.IsSequence()) {
            BOOST_LOG_TRIVIAL(fatal)
                << "Workload is a map, but nodes is not sequnce in workload type "
                   "initializer ";
            exit(EXIT_FAILURE);
        }
        if (inputNodes["seed"]) {
            rng.seed(inputNodes["seed"].as<uint64_t>());
            BOOST_LOG_TRIVIAL(debug) << " Random seed: " << inputNodes["seed"].as<uint64_t>();
        }
        if (inputNodes["wvariables"]) {
            // read in any variables
            for (auto var : inputNodes["wvariables"]) {
                BOOST_LOG_TRIVIAL(debug) << "Reading in workload variable " << var.first.Scalar()
                                         << " with value " << var.second.Scalar();
                wvariables[var.first.Scalar()] = var.second.as<int64_t>();
            }
        }
        if (inputNodes["tvariables"]) {
            // read in any variables
            for (auto var : inputNodes["tvariables"]) {
                cout << "Reading in thread variable " << var.first.Scalar() << " with value "
                     << var.second.Scalar();
                tvariables[var.first.Scalar()] = var.second.as<int64_t>();
            }
        }
        name = inputNodes["name"].Scalar();
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor, and was passed in a map. Name: "
                                 << name;
        if (inputNodes["threads"]) {
            numParallelThreads = inputNodes["threads"].as<uint64_t>();
            BOOST_LOG_TRIVIAL(debug) << "Excplicity setting number of threads in workload";
        } else
            BOOST_LOG_TRIVIAL(debug) << "Using default value for number of threads";
        if (inputNodes["runLength"]) {
            runLength = inputNodes["runLength"].as<uint64_t>();
            BOOST_LOG_TRIVIAL(debug) << "Excplicity setting runLength in workload";
        } else
            BOOST_LOG_TRIVIAL(debug) << "Using default value for runLength";
    } else if (inputNodes.IsSequence()) {
        yamlNodes = inputNodes;
    } else {
        BOOST_LOG_TRIVIAL(fatal) << "Not sequnce in workload type initializer";
        exit(EXIT_FAILURE);
    }
    if (!yamlNodes.IsSequence()) {
        BOOST_LOG_TRIVIAL(fatal)
            << "Not sequnce in workload type initializer after passing through "
               "map or sequence processing. Type is " << yamlNodes.Type();
        exit(EXIT_FAILURE);
    }

    for (auto yamlNode : yamlNodes) {
        if (!yamlNode.IsMap()) {
            BOOST_LOG_TRIVIAL(fatal) << "Node in workload is not a yaml map";
            exit(EXIT_FAILURE);
        }
        if (yamlNode["type"].Scalar() == "opNode") {
            auto mynode = make_shared<opNode>(yamlNode);
            nodes[mynode->getName()] = mynode;
            // this is an ugly hack for now
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added op node";
        } else if (yamlNode["type"].Scalar() == "random_choice") {
            auto mynode = make_shared<random_choice>(yamlNode);
            nodes[mynode->getName()] = mynode;
            // this is an ugly hack for now
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added random_choice node";
        } else if (yamlNode["type"].Scalar() == "sleep") {
            auto mynode = make_shared<sleepNode>(yamlNode);
            nodes[mynode->getName()] = mynode;
            // this is an ugly hack for now
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added sleep node";
        } else if (yamlNode["type"].Scalar() == "forN") {
            auto mynode = make_shared<forN>(yamlNode);
            nodes[mynode->getName()] = mynode;
            // this is an ugly hack for now
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added sleep node";
        } else if (yamlNode["type"].Scalar() == "finish") {
            auto mynode = make_shared<finishNode>(yamlNode);
            nodes[mynode->getName()] = mynode;
            // this is an ugly hack for now
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added finish node";
        } else {
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor. Defaulting to opNode";
            auto mynode = make_shared<opNode>(yamlNode);
            nodes[mynode->getName()] = mynode;
            vectornodes.push_back(mynode);
            BOOST_LOG_TRIVIAL(debug) << "In workload constructor. Added opNode";
        }
    }
    BOOST_LOG_TRIVIAL(debug) << "Added all the nodes in yamlNode";
    // Add an implicit finish_node if it doesn't exist
    if (!nodes["Finish"]) {
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode;
        // this is an ugly hack for now
        vectornodes.push_back(mynode);
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added implicit finish node";
    }

    // link the things together
    for (auto mnode : vectornodes) {
        BOOST_LOG_TRIVIAL(debug) << "Setting next node for " << mnode->getName()
                                 << ". Next node name is " << mnode->nextName;
        mnode->setNextNode(nodes);
    }
}
void runThread(shared_ptr<node> Node, shared_ptr<threadState> myState) {
    myState->currentNode = Node;
    Node->executeNode(myState);
}
void workload::execute(mongocxx::client& conn) {
    // prep the threads and start them. Should put the timer in here also.
    vector<thread> myThreads;
    for (int i = 0; i < numParallelThreads; i++) {
        // create thread state for each
        auto newState = shared_ptr<threadState>(new threadState(rng(), tvariables, wvariables));
        threads.insert(newState);
        myThreads.push_back(thread(runThread, vectornodes[0], newState));
    }
    // wait for all the threads to finish
    for (int i = 0; i < numParallelThreads; i++) {
        // clean up the thread state?
        myThreads[i].join();
    }
}
void workload::stop() {
    stopped = true;
    for (auto mnode : vectornodes)
        mnode->stop();
};
}
