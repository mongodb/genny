#include <stdlib.h>
#include <iostream>
#include <thread>
#include <boost/log/trivial.hpp>
#include <time.h>
#include <chrono>
#include "finish_node.hpp"
#include "parse_util.hpp"

#include <mongocxx/instance.hpp>

#include "workload.hpp"
#include "node.hpp"

namespace mwg {
workload::workload(YAML::Node& inputNodes) : stopped(false) {
    if (!inputNodes) {
        BOOST_LOG_TRIVIAL(fatal) << "Workload constructor and !nodes";
        exit(EXIT_FAILURE);
    }
    unordered_map<string, bsoncxx::array::value> tvariables;  // thread variables
    unordered_map<string, shared_ptr<node>> nodes;
    YAML::Node yamlNodes;
    if (inputNodes.IsMap()) {
        // read out things like the seed
        yamlNodes = inputNodes["nodes"];
        name = inputNodes["name"].Scalar();
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor, and was passed in a map. Name: "
                                 << name;
        if (!yamlNodes.IsSequence()) {
            BOOST_LOG_TRIVIAL(fatal)
                << "Workload is a map, but nodes is not sequnce in workload type "
                   "initializer ";
            exit(EXIT_FAILURE);
        }
        if (inputNodes["wvariables"]) {
            // read in any variables
            for (auto var : inputNodes["wvariables"]) {
                BOOST_LOG_TRIVIAL(debug) << "Reading in workload variable " << var.first.Scalar()
                                         << " with value " << var.second.Scalar();
                wvariables.insert({var.first.Scalar(), yamlToValue(var.second)});
            }
        }
        if (inputNodes["tvariables"]) {
            // read in any variables
            for (auto var : inputNodes["tvariables"]) {
                cout << "Reading in thread variable " << var.first.Scalar() << " with value "
                     << var.second.Scalar();
                tvariables.insert({var.first.Scalar(), yamlToValue(var.second)});
            }
        }
        myState = unique_ptr<threadState>(
            new threadState(0, tvariables, wvariables, *this, "testDB", "testCollection"));
        if (inputNodes["seed"]) {
            myState->rng.seed(inputNodes["seed"].as<uint64_t>());
            BOOST_LOG_TRIVIAL(debug) << " Random seed: " << inputNodes["seed"].as<uint64_t>();
        }
        if (inputNodes["database"]) {
            myState->DBName = inputNodes["database"].Scalar();
            BOOST_LOG_TRIVIAL(debug) << "In Workload constructor and database name is "
                                     << myState->DBName;
        }
        if (inputNodes["collection"]) {
            myState->CollectionName = inputNodes["collection"].Scalar();
            BOOST_LOG_TRIVIAL(debug) << "In Workload constructor and collection name is "
                                     << myState->CollectionName;
        }
        if (inputNodes["threads"]) {
            numParallelThreads = inputNodes["threads"].as<int64_t>();
            BOOST_LOG_TRIVIAL(debug) << "Explicitly setting number of threads in workload to "
                                     << numParallelThreads;
        } else {
            numParallelThreads = 1;
            BOOST_LOG_TRIVIAL(debug) << "Using default value for number of threads";
        }
        if (inputNodes["runLength"]) {
            runLength = inputNodes["runLength"].as<uint64_t>();
            BOOST_LOG_TRIVIAL(debug) << "Explicitly setting runLength in workload";
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
        auto mynode = makeSharedNode(yamlNode);
        nodes[mynode->getName()] = mynode;
        // this is an ugly hack for now
        vectornodes.push_back(mynode);
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added node";
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
        mnode->setNextNode(nodes, vectornodes);
    }
}

class timerState {
public:
    timerState(workload& work) : myWork(work), done(false){};
    workload& myWork;
    std::mutex mut;
    bool done;
};

// Function to start a timer.
void runTimer(shared_ptr<timerState> state, uint64_t runLength) {
    if (runLength == 0)
        return;
    std::this_thread::sleep_for(std::chrono::seconds(runLength));
    {
        // grab lock before checking state
        std::lock_guard<std::mutex> lk(state->mut);
        if (!state->done) {
            state->myWork.stop();
        }
    }
}

void workload::execute() {
    // prep the threads and start them. Should put the timer in here also.
    BOOST_LOG_TRIVIAL(trace) << "In workload::execute";
    vector<thread> myThreads;

    // setup timeout
    BOOST_LOG_TRIVIAL(trace) << "RunLength is " << runLength << ". About to setup timer";
    auto ts = shared_ptr<timerState>(new timerState(*this));
    std::thread timer(runTimer, ts, runLength);

    chrono::high_resolution_clock::time_point start, stop;
    start = chrono::high_resolution_clock::now();
    // local copy in case it changes from multiple calls
    int64_t numThreads = numParallelThreads;
    BOOST_LOG_TRIVIAL(debug) << "Starting " << numThreads << " threads";
    for (int64_t i = 0; i < numParallelThreads; i++) {
        BOOST_LOG_TRIVIAL(trace) << "Starting thread in workload";
        // create thread state for each
        auto newState = shared_ptr<threadState>(new threadState(myState->rng(),
                                                                myState->tvariables,
                                                                myState->wvariables,
                                                                *this,
                                                                myState->DBName,
                                                                myState->CollectionName,
                                                                uri));
        BOOST_LOG_TRIVIAL(trace) << "Created thread state";
        threads.insert(newState);
        myThreads.push_back(thread(runThread, vectornodes[0], newState));
        BOOST_LOG_TRIVIAL(trace) << "Called run on thread";
    }
    BOOST_LOG_TRIVIAL(trace) << "Started all threads in workload";
    // wait for all the threads to finish
    for (int64_t i = 0; i < numThreads; i++) {
        // clean up the thread state?
        myThreads[i].join();
    }
    stop = chrono::high_resolution_clock::now();
    myStats.record(std::chrono::duration_cast<chrono::microseconds>(stop - start));
    BOOST_LOG_TRIVIAL(trace) << "All threads finished. About to stop timer";

    // clean up the timer stuff
    {
        std::lock_guard<std::mutex> lk(ts->mut);
        ts->done = true;
        timer.detach();
    }

    BOOST_LOG_TRIVIAL(debug) << "Workload " << name << " took "
                             << std::chrono::duration_cast<chrono::milliseconds>(stop - start)
                                    .count() << " milliseconds";
}
void workload::stop() {
    stopped = true;
    for (auto mnode : vectornodes)
        mnode->stop();
}

void workload::logStats() {
    if (myStats.getCount() > 0)
        BOOST_LOG_TRIVIAL(info) << "Workload: " << name << ", Count=" << myStats.getCount()
                                << ", Avg=" << myStats.getMean().count()
                                << "us, Min=" << myStats.getMin().count()
                                << "us, Max = " << myStats.getMax().count() << "us";
    for (auto mnode : vectornodes)
        mnode->logStats();
}

bsoncxx::document::value workload::getStats(bool withReset) {
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    bsoncxx::builder::stream::document document{};

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that could be
    // moved into document
    auto stats = myStats.getStats(withReset);
    document << name << open_document << bsoncxx::builder::concatenate(stats.view());
    for (auto mnode : vectornodes) {
        auto nstats = mnode->getStats(withReset);
        document << bsoncxx::builder::concatenate(nstats.view());
    }
    document << "Date" << bsoncxx::types::b_date(std::chrono::system_clock::now());
    document << close_document;
    return (document << bsoncxx::builder::stream::finalize);
}


std::string workload::generateDotGraph() {
    string extra;
    string nodes = "digraph " + name + " {\n";
    for (auto node : vectornodes) {
        auto graph = node->generateDotGraph();
        nodes += graph.first;
        extra += graph.second;
    }
    nodes += "}\n";
    return (nodes + extra);
}
}
