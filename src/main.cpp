#include <iostream>
#include <fstream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array_context.hpp>
#include <bsoncxx/builder/stream/single_context.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>
#include <getopt.h>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "workload.hpp"
#include "main.h"
#include <chrono>

using namespace std;
using namespace mwg;
namespace logging = boost::log;

static struct option poptions[] = {{"help", no_argument, 0, 'h'},
                                   {"loglevel", required_argument, 0, 'l'},
                                   {"dotfile", required_argument, 0, 'd'},
                                   {"resultsfile", required_argument, 0, 'r'},
                                   {"host", required_argument, 0, 0},
                                   {"resultsperiod", required_argument, 0, 'p'},
                                   {"version", no_argument, 0, 'v'},
                                   {0, 0, 0, 0}};

void print_help(const char* process_name) {
    fprintf(stderr,
            "Usage: %s [-hldrpv] /path/to/workload [workload to run]\n"
            "Execution Options:\n"
            "\t--help|-h              Display this help and exit\n"
            "\t--host Host            Host/Connection string for mongo server to test--must be a\n"
            "\t                       full URI,\n"
            "\t--loglevel|-l LEVEL    Set the logging level. Valid options are trace,\n"
            "\t                       debug, info, warning, error, and fatal.\n"
            "\t--dotfile|-d FILE      Generate dotfile to FILE from workload and exit.\n"
            "\t                       WARNING: names with spaces or other special characters\n"
            "\t                       will break the dot file\n\n"
            "\t--resultfile|-r FILE   FILE to store results to. defaults to results.json\n"
            "\t--resultsperiod|-p SEC Record results every SEC seconds\n"
            "\t--version|-v           Return version information\n",
            process_name);
}

class statsState {
public:
    statsState(workload& work) : myWorkload(work), done(false){};
    workload& myWorkload;
    std::atomic<bool> done;
};

void runPeriodicStats(shared_ptr<statsState> state, std::chrono::seconds period, string outFile) {
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
    string dotFile;
    string resultsFile = "results.json";
    std::chrono::seconds resultPeriod(0);
    string uri = mongocxx::uri::k_default_uri;
    int arg_count = 0;
    int idx = 0;

    // default logging level to info
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);

    while (1) {
        int arg = getopt_long(argc, argv, "hl:d:p:v", poptions, &idx);
        arg_count++;
        if (arg == -1) {
            // all arguments have been processed
            break;
        }
        ++arg_count;

        switch (arg) {
            case 0:
                switch (idx) {
                    case 4:
                        uri = optarg;
                        break;
                    default:
                        fprintf(stderr, "unknown command line option with optarg index %d\n", idx);
                        return EXIT_FAILURE;
                }
                break;
            case 'h':
                print_help(argv[0]);
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
                        GitHASH);
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "unknown command line option: %s\n", poptions[idx].name);
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc > optind) {
        fileName = argv[optind];
        if (argc > optind + 1) {
            workloadName = argv[optind + 1];
        }
    } else {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }
    BOOST_LOG_TRIVIAL(info) << fileName;

    // put try catch here with error message
    YAML::Node nodes = YAML::LoadFile(fileName);

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
        myWorkloadState.uri = uri;
        auto ss = shared_ptr<statsState>(new statsState(myWorkload));
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
