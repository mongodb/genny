#include "main.h"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <bson.h>
#include <bsoncxx/builder/stream/array_context.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/single_context.hpp>
#include <bsoncxx/json.hpp>
#include <chrono>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <string>
#include <yaml-cpp/yaml.h>
#include <vector>

#include "workload.hpp"

using namespace std;
using namespace mwg;
namespace logging = boost::log;

static struct option poptions[] = {{"collection", required_argument, 0, 0},
                                   {"database", required_argument, 0, 0},
                                   {"dotfile", required_argument, 0, 'd'},
                                   {"help", no_argument, 0, 'h'},
                                   {"host", required_argument, 0, 0},
                                   {"loglevel", required_argument, 0, 'l'},
                                   {"numThreads", required_argument, 0, 0},
                                   {"resultsfile", required_argument, 0, 'r'},
                                   {"resultsperiod", required_argument, 0, 'p'},
                                   {"runLengthMs", required_argument, 0, 0},
                                   {"variable", required_argument, 0, 0},
                                   {"version", no_argument, 0, 'v'},
                                   {0, 0, 0, 0}};

void printHelp(const char* processName) {
    fprintf(
        stderr,
        "Usage: %s [-hldrpv] /path/to/workload [workload to run]\n"
        "Execution Options:\n"
        "\t--collection COLL      Use Collection name COLL by default\n"
        "\t--database DB          Use Database name DB by default\n"
        "\t--dotfile|-d FILE      Generate dotfile to FILE from workload and exit.\n"
        "\t                       WARNING: names with spaces or other special characters\n"
        "\t                       will break the dot file\n"
        "\t--help|-h              Display this help and exit\n"
        "\t--host Host            Host/Connection string for mongo server to test--must be a\n"
        "\t                       full URI,\n"
        "\t--loglevel|-l LEVEL    Set the logging level. Valid options are trace,\n"
        "\t                       debug, info, warning, error, and fatal.\n"
        "\t--numThreads NUM       Run the workload with NUM threads instead of number\n"
        "\t                       specified in yaml file\n"
        "\t--resultfile|-r FILE   FILE to store results to. defaults to results.json\n"
        "\t--resultsperiod|-p SEC Record results every SEC seconds\n"
        "\t--runLengthMs NUM      Run the workload for up to NUM milliseconds instead of length\n"
        "\t                       specified in yaml file\n"
        "\t--variable VAR=VALUE   Override the value of yaml node VAR with VALUE. May be called\n"
        "\t                       multiple times. If you override a node that defines a YAML\n"
        "\t                       anchor, all aliases to that anchor will get the new value\n"
        "\t--version|-v           Return version information\n",
        processName);
}

class StatsState {
public:
    StatsState(workload& work) : myWorkload(work), done(false){};
    workload& myWorkload;
    std::atomic<bool> done;
};

void runPeriodicStats(shared_ptr<StatsState> state, std::chrono::seconds period, string outFile) {
    // check if we should be doing something.
    if (period == std::chrono::seconds(0)) {
        return;
    }
    ofstream out;
    bool haveFile = false;
    if (outFile.length() > 0) {
        out.open(outFile);
        haveFile = true;  // should error check here
        out << "[";
    }

    std::this_thread::sleep_for(period);
    while (!state->done) {
        chrono::high_resolution_clock::time_point start, stop;
        start = chrono::high_resolution_clock::now();
        state->myWorkload.logStats();
        if (haveFile) {
            out << bsoncxx::to_json(state->myWorkload.getStats(true)) << ",\n";
        }
        stop = chrono::high_resolution_clock::now();
        BOOST_LOG_TRIVIAL(debug) << "Periodic stats collection took "
                                 << std::chrono::duration_cast<std::chrono::microseconds>(
                                        stop - start).count() << " us";
        std::this_thread::sleep_for(period);
    }
    state->myWorkload.logStats();
    if (haveFile)
        out << bsoncxx::to_json(state->myWorkload.getStats(true)) << "]\n";
}

int main(int argc, char* argv[]) {
    string fileName = "sample.yml";
    string workloadName = "main";
    string databaseName = "";
    string collectionName = "";
    string dotFile;
    string resultsFile = "results.json";
    std::chrono::seconds resultPeriod(0);
    string uri = mongocxx::uri::k_default_uri;
    int64_t numThreads = -1;  // default not used
    int64_t runLengthMS = -1;
    int argCount = 0;
    int idx = 0;
    vector<pair<string, string>> variableOverrides;

    // default logging level to info
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);

    while (1) {
        int arg = getopt_long(argc, argv, "hl:d:p:v", poptions, &idx);
        argCount++;
        if (arg == -1) {
            // all arguments have been processed
            break;
        }
        ++argCount;

        // variables for override
        string myString, variable, value;
        int indexOfEquals;

        switch (arg) {
            case 0:
                switch (idx) {
                    case 0:
                        collectionName = optarg;
                        break;
                    case 1:
                        databaseName = optarg;
                        break;
                    case 4:
                        uri = optarg;
                        break;
                    case 6:
                        numThreads = atoi(optarg);
                        break;
                    case 9:
                        runLengthMS = atoi(optarg);
                        break;
                    case 10:
                        myString = string(optarg);
                        indexOfEquals = myString.find('=');
                        if (indexOfEquals == string::npos) {
                            fprintf(stderr, "Variable override does not contain '=': %s", optarg);
                            return EXIT_FAILURE;
                        }
                        variable = myString.substr(0, indexOfEquals);
                        value = myString.substr(indexOfEquals + 1, myString.length());
                        variableOverrides.push_back(pair<string, string>(variable, value));
                        break;
                    default:
                        fprintf(stderr, "unknown command line option with optarg index %d\n", idx);
                        return EXIT_FAILURE;
                }
                break;
            case 'h':
                printHelp(argv[0]);
                return EXIT_SUCCESS;
            case 'l':
                if (strcmp("info", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::info);
                else if (strcmp("trace", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::trace);
                else if (strcmp("debug", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::debug);
                else if (strcmp("warning", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::warning);
                else if (strcmp("error", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::error);
                else if (strcmp("fatal", optarg) == 0)
                    logging::core::get()->set_filter(logging::trivial::severity >=
                                                     logging::trivial::fatal);
                break;
            case 'd':
                dotFile = optarg;
                break;
            case 'r':
                resultsFile = optarg;
                break;
            case 'p':
                resultPeriod = std::chrono::seconds(atoi(optarg));
                break;
            case 'v':
                fprintf(stdout,
                        "mwg version %d.%d. Githash %s\n",
                        WorkloadGen_VERSION_MAJOR,
                        WorkloadGen_VERSION_MINOR,
                        g_GIT_SHA1);
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "unknown command line option: %s\n", poptions[idx].name);
                printHelp(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc > optind) {
        fileName = argv[optind];
        if (argc > optind + 1) {
            workloadName = argv[optind + 1];
        }
    } else {
        printHelp(argv[0]);
        return EXIT_SUCCESS;
    }
    BOOST_LOG_TRIVIAL(info) << fileName;

    // put try catch here with error message
    YAML::Node nodes = YAML::LoadFile(fileName);

    // put in overrides
    for (auto override : variableOverrides) {
        BOOST_LOG_TRIVIAL(info) << "Changing yaml node " << override.first << " to "
                                << override.second << " based on command line";
        if (!nodes[override.first]) {
            BOOST_LOG_TRIVIAL(fatal) << override.first << " does not exist in the YAML file\n";
            return EXIT_FAILURE;
        }
        nodes[override.first] = override.second;
    }

    // Look for main. And start building from there.
    if (auto main = nodes[workloadName]) {
        workload myWorkload(main);
        if (dotFile.length() > 0) {
            // save the dotgraph
            ofstream dotout;
            dotout.open(dotFile);
            dotout << myWorkload.generateDotGraph();
            dotout.close();
            return EXIT_SUCCESS;
        }

        BOOST_LOG_TRIVIAL(trace) << "After workload constructor. Before execute";
        // set the uri
        WorkloadExecutionState myWorkloadState = myWorkload.newWorkloadState();
        if (numThreads > 0)
            myWorkloadState.numParallelThreads = numThreads;
        if (runLengthMS > 0)
            myWorkloadState.runLengthMs = runLengthMS;
        if (collectionName != "")
            myWorkloadState.CollectionName = collectionName;
        if (databaseName != "")
            myWorkloadState.DBName = databaseName;
        myWorkloadState.uri = uri;
        auto ss = shared_ptr<StatsState>(new StatsState(myWorkload));
        std::thread stats(runPeriodicStats, ss, resultPeriod, resultsFile);
        myWorkload.execute(myWorkloadState);
        ss->done = true;
        stats.join();
        myWorkload.logStats();
        if (resultPeriod == std::chrono::seconds::zero() && resultsFile.length() > 0) {
            // save the results
            ofstream out;
            out.open(resultsFile);
            out << bsoncxx::to_json(myWorkload.getStats(false));
            out.close();
        }
    }

    else
        BOOST_LOG_TRIVIAL(fatal) << "There was no main";
}
