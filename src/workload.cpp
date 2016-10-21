#include <boost/log/trivial.hpp>
#include <chrono>
#include <iostream>
#include <mongocxx/instance.hpp>
#include <stdlib.h>
#include <thread>
#include <time.h>


#include "finish_node.hpp"
#include "node.hpp"
#include "parse_util.hpp"
#include "workload.hpp"

namespace mwg {

static int workloadCount = 0;

void WorkloadExecutionState::increaseThreads() {
    uint64_t numThreads;
    {
        // Get lock
        std::lock_guard<std::mutex> lk(threadMut);
        // increase numActiveThreads
        numActiveThreads++;
        numThreads = numActiveThreads;
    }
    BOOST_LOG_TRIVIAL(trace)
        << "In WorkloadExecutionState::increaseThreads and increased threads to " << numThreads;
}

void WorkloadExecutionState::decreaseThreads() {
    uint64_t numThreads;
    {
        // Get lock
        std::lock_guard<std::mutex> lk(threadMut);
        if (numActiveThreads == 0) {
            BOOST_LOG_TRIVIAL(fatal) << "In WorkloadExecutionState::decreaseThreads but "
                                        "numActiveThreads is already zero";
            exit(EXIT_FAILURE);
        }
        // decrease numActiveThreads
        numActiveThreads--;
        numThreads = numActiveThreads;
    }
    BOOST_LOG_TRIVIAL(trace)
        << "In WorkloadExecutionState::decreaseThreads and decreased threads to " << numThreads;
    // notify
    if (numThreads == 0)
        threadsDoneCV.notify_one();
}

uint64_t WorkloadExecutionState::getActiveThreads() {
    std::lock_guard<std::mutex> lk(threadMut);
    return (numActiveThreads);
}

bool WorkloadExecutionState::anyThreadsActive() {
    std::lock_guard<std::mutex> lk(threadMut);
    return (numActiveThreads > 0);
}

void WorkloadExecutionState::waitThreadsDone() {
    BOOST_LOG_TRIVIAL(trace) << "In WorkloadExecutionState::waitThreadsDone()";
    std::unique_lock<std::mutex> lk(threadMut);
    while (numActiveThreads > 0) {
        BOOST_LOG_TRIVIAL(trace)
            << "In WorkloadExecutionState::waitThreadsDone() while loop -- threads > 0: "
            << numActiveThreads;
        threadsDoneCV.wait(lk);
    }
    return;
}


workload::workload(const YAML::Node& inputNodes) : baseWorkloadState(this), stopped(false) {
    if (!inputNodes) {
        BOOST_LOG_TRIVIAL(fatal) << "Workload constructor and !nodes";
        exit(EXIT_FAILURE);
    }
    unordered_map<string, node*> nodes;
    YAML::Node yamlNodes;
    if (inputNodes.IsMap()) {
        // read out things like the seed
        yamlNodes = inputNodes["nodes"];
        if (inputNodes["name"]) {
            name = inputNodes["name"].Scalar();
            BOOST_LOG_TRIVIAL(trace) << "Set workload name to explicit name: " << name;
        } else {
            name = "Workload" + std::to_string(workloadCount++);  // default name
            BOOST_LOG_TRIVIAL(trace) << "Set workload name to default name: " << name;
        }
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
                baseWorkloadState.wvariables.insert({var.first.Scalar(), yamlToValue(var.second)});
            }
        }
        if (inputNodes["tvariables"]) {
            // read in any variables
            for (auto var : inputNodes["tvariables"]) {
                BOOST_LOG_TRIVIAL(debug) << "Reading in thread variable " << var.first.Scalar()
                                         << " with value " << var.second.Scalar();
                tvariables.insert({var.first.Scalar(), yamlToValue(var.second)});
            }
        }
        if (inputNodes["seed"]) {
            baseWorkloadState.rng.seed(inputNodes["seed"].as<uint64_t>());
            BOOST_LOG_TRIVIAL(debug) << " Random seed: " << inputNodes["seed"].as<uint64_t>();
        }
        if (inputNodes["database"]) {
            baseWorkloadState.DBName = inputNodes["database"].Scalar();
            BOOST_LOG_TRIVIAL(debug) << "In Workload constructor and database name is "
                                     << baseWorkloadState.DBName;
        }
        if (inputNodes["collection"]) {
            baseWorkloadState.CollectionName = inputNodes["collection"].Scalar();
            BOOST_LOG_TRIVIAL(debug) << "In Workload constructor and collection name is "
                                     << baseWorkloadState.CollectionName;
        }
        if (inputNodes["threads"]) {
            baseWorkloadState.numParallelThreads = inputNodes["threads"].as<int64_t>();
            BOOST_LOG_TRIVIAL(debug) << "Explicitly setting number of threads in workload to "
                                     << baseWorkloadState.numParallelThreads;
        } else {
            baseWorkloadState.numParallelThreads = 1;
            BOOST_LOG_TRIVIAL(debug) << "Using default value for number of threads";
        }
        if (inputNodes["runLengthMs"]) {
            baseWorkloadState.runLengthMs = inputNodes["runLengthMs"].as<uint64_t>();
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
               "map or sequence processing. Type is "
            << yamlNodes.Type();
        exit(EXIT_FAILURE);
    }

    for (auto yamlNode : yamlNodes) {
        if (!yamlNode.IsMap()) {
            BOOST_LOG_TRIVIAL(fatal) << "Node in workload is not a yaml map";
            exit(EXIT_FAILURE);
        }
        auto mynode = makeSharedNode(yamlNode);
        nodes[mynode->getName()] = mynode.get();
        // this is an ugly hack for now
        vectornodes.push_back(std::move(mynode));
        BOOST_LOG_TRIVIAL(debug) << "In workload constructor and added node";
    }
    BOOST_LOG_TRIVIAL(debug) << "Added all the nodes in yamlNode";
    // Add an implicit finish_node if it doesn't exist
    if (!nodes["Finish"]) {
        auto mynode = make_shared<finishNode>();
        nodes[mynode->getName()] = mynode.get();
        // this is an ugly hack for now
        vectornodes.push_back(std::move(mynode));
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
    timerState(workload* work) : myWork(work), done(false){};
    workload* myWork;
    std::mutex mut;
    bool done;
};

// Function to start a timer.
void runTimer(shared_ptr<timerState> state, uint64_t runLengthMs) {
    if (runLengthMs == 0)
        return;
    std::this_thread::sleep_for(std::chrono::milliseconds(runLengthMs));
    {
        // grab lock before checking state
        std::lock_guard<std::mutex> lk(state->mut);
        if (!state->done) {
            state->myWork->stop();
        }
    }
}

// wrapper to start a new thread
void runThread(node* Node, shared_ptr<threadState> myState) {
    BOOST_LOG_TRIVIAL(trace) << "Node runThread";
    myState->currentNode = Node;
    BOOST_LOG_TRIVIAL(trace) << "Set node. Name is " << Node->name;
    while (myState->currentNode != nullptr)
        myState->currentNode->executeNode(myState);
    // I'm done. Decrease the count of threads
    myState->workloadState->decreaseThreads();
}

// Start a new thread with thread state and initial state
shared_ptr<thread> startThread(node* startNode, shared_ptr<threadState> ts) {
    // increase the count of threads
    ts->workloadState->increaseThreads();
    return (shared_ptr<thread>(new thread(runThread, startNode, ts)));
}


void workload::execute(WorkloadExecutionState* work) {
    // prep the threads and start them. Should put the timer in here also.
    BOOST_LOG_TRIVIAL(trace) << "In workload::execute";

    // setup timeout
    BOOST_LOG_TRIVIAL(trace) << "RunLength is " << work->runLengthMs << ". About to setup timer";
    auto ts = shared_ptr<timerState>(new timerState(this));
    std::thread timer(runTimer, ts, work->runLengthMs);

    chrono::high_resolution_clock::time_point start, stop;
    start = chrono::high_resolution_clock::now();
    auto numParallelThreads = work->numParallelThreads;
    BOOST_LOG_TRIVIAL(debug) << "Starting " << numParallelThreads << " threads";
    for (uint64_t i = 0; i < numParallelThreads; i++) {
        BOOST_LOG_TRIVIAL(trace) << "Starting thread in workload";
        // create thread state for each
        auto newState = shared_ptr<threadState>(new threadState(work->rng(),
                                                                tvariables,
                                                                work->wvariables,
                                                                work,
                                                                work->DBName,
                                                                work->CollectionName,
                                                                work->uri));
        BOOST_LOG_TRIVIAL(trace) << "Created thread state";
        // Start the thread
        startThread(vectornodes[0].get(), newState)->detach();
        BOOST_LOG_TRIVIAL(trace) << "Called run on thread";
    }
    BOOST_LOG_TRIVIAL(trace) << "Started all threads in workload";
    // wait for all the threads to finish
    work->waitThreadsDone();
    stop = chrono::high_resolution_clock::now();
    // need to put a lock around this
    myStats.recordMicros(std::chrono::duration_cast<chrono::microseconds>(stop - start));
    BOOST_LOG_TRIVIAL(trace) << "All threads finished. About to stop timer";

    // clean up the timer stuff
    {
        std::lock_guard<std::mutex> lk(ts->mut);
        ts->done = true;
        timer.detach();
    }

    BOOST_LOG_TRIVIAL(debug)
        << "Workload " << name << " took "
        << std::chrono::duration_cast<chrono::milliseconds>(stop - start).count()
        << " milliseconds";
}
void workload::stop() {
    stopped = true;
    for (auto mnode : vectornodes)
        mnode->stop();
}

void workload::logStats() {
    if (myStats.getCount() > 0)
        BOOST_LOG_TRIVIAL(info) << "Workload: " << name << ", Count=" << myStats.getCount()
                                << ", Avg=" << myStats.getMeanMicros().count()
                                << "us, Min=" << myStats.getMinimumMicros().count()
                                << "us, Max = " << myStats.getMaximumMicros().count() << "us";
    for (auto mnode : vectornodes)
        mnode->logStats();
}

bsoncxx::document::value workload::getStats(bool withReset) {
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;
    bsoncxx::builder::stream::document document{};

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that
    // could be
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
