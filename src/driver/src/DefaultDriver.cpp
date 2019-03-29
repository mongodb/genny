// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <yaml-cpp/yaml.h>

#include <loki/ScopeGuard.h>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

#include <driver/DefaultDriver.hpp>

namespace genny::driver {
namespace {

using namespace genny;

YAML::Node loadConfig(const std::string& source,
                      DefaultDriver::ProgramOptions::YamlSource sourceType) {
    if (sourceType == DefaultDriver::ProgramOptions::YamlSource::kString) {
        return YAML::Load(source);
    }
    try {
        return YAML::LoadFile(source);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << source << ": " << ex.what();
        throw;
    }
}

template <typename Actor>
void runActor(Actor&& actor,
              std::atomic<driver::DefaultDriver::OutcomeCode>& outcomeCode,
              Orchestrator& orchestrator) {
    auto guard = Loki::MakeGuard([&]() { orchestrator.abort(); });

    try {
        actor->run();
    } catch (const boost::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected boost::exception: "
                                 << boost::diagnostic_information(x, true);
        outcomeCode = driver::DefaultDriver::OutcomeCode::kBoostException;
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected std::exception: " << x.what();
        outcomeCode = driver::DefaultDriver::OutcomeCode::kStandardException;
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Unknown error";
        // Don't try to handle unknown errors, let us crash ungracefully
        throw;
    }
}

DefaultDriver::OutcomeCode doRunLogic(const DefaultDriver::ProgramOptions& options) {
    genny::metrics::Registry metrics;
    auto actorSetup = metrics.operation("Genny", "Setup", 0u);
    auto setupCtx = actorSetup.start();

    if (options.runMode == DefaultDriver::RunMode::kListActors) {
        globalCast().streamProducersTo(std::cout);
        setupCtx.success();
        return DefaultDriver::OutcomeCode::kSuccess;
    }

    auto yaml = loadConfig(options.workloadSource, options.workloadSourceType);
    auto orchestrator = Orchestrator{};

    if (options.runMode == DefaultDriver::RunMode::kEvaluate) {
        std::cout << "Printing evaluated workload YAML file:" << std::endl;
        std::cout << YAML::Dump(yaml) << std::endl;
        setupCtx.success();
        return DefaultDriver::OutcomeCode::kSuccess;
    }

    auto workloadContext =
        WorkloadContext{yaml, metrics, orchestrator, options.mongoUri, globalCast()};

    if (options.runMode == DefaultDriver::RunMode::kDryRun) {
        std::cout << "Workload context constructed without errors." << std::endl;
        setupCtx.success();
        return DefaultDriver::OutcomeCode::kSuccess;
    }

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));

    setupCtx.success();

    auto startedActors = metrics.operation("Genny", "ActorStarted", 0u);
    auto finishedActors = metrics.operation("Genny", "ActorFinished", 0u);

    std::atomic<DefaultDriver::OutcomeCode> outcomeCode = DefaultDriver::OutcomeCode::kSuccess;

    std::mutex reporting;
    std::vector<std::thread> threads;
    std::transform(cbegin(workloadContext.actors()),
                   cend(workloadContext.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) {
                       return std::thread{[&]() {
                           {
                               auto ctx = startedActors.start();
                               ctx.addDocuments(1);

                               std::lock_guard<std::mutex> lk{reporting};
                               ctx.success();
                           }

                           runActor(actor, outcomeCode, orchestrator);

                           {
                               auto ctx = finishedActors.start();
                               ctx.addDocuments(1);

                               std::lock_guard<std::mutex> lk{reporting};
                               ctx.success();
                           }
                       }};
                   });

    for (auto& thread : threads)
        thread.join();

    const auto reporter = genny::metrics::Reporter{metrics};

    std::ofstream metricsOutput;
    metricsOutput.open(options.metricsOutputFileName, std::ofstream::out | std::ofstream::trunc);
    reporter.report(metricsOutput, options.metricsFormat);
    metricsOutput.close();

    return outcomeCode;
}

}  // namespace


DefaultDriver::OutcomeCode DefaultDriver::run(const DefaultDriver::ProgramOptions& options) const {
    try {
        // Wrap doRunLogic in another catch block in case it throws an exception of its own e.g.
        // file not found or io errors etc - exceptions not thrown by ActorProducers.
        return doRunLogic(options);
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Caught exception " << x.what();
    }
    return DefaultDriver::OutcomeCode::kInternalException;
}


namespace {

/**
 * Normalize the metrics output file command-line option.
 *
 * @param str the input option value from the command-lien
 * @return the file-path that should be used to open the output stream.
 */
// There may be a more conventional way to define conversion/normalization
// functions for use with boost::program_options. The tutorial isn't the
// clearest thing. If we need to do more than 1-2, look into that further.
std::string normalizeOutputFile(const std::string& str) {
    if (str == "-") {
        return std::string("/dev/stdout");
    }
    return str;
}

}  // namespace


DefaultDriver::ProgramOptions::ProgramOptions(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description progDescription{u8"🧞‍ Allowed Options 🧞‍"};
    po::positional_options_description positional;

    // clang-format off
    const auto runModeHelp = u8R"(
Genny run modes
    normal
        Run the workload normally; default mode if no run mode is specified

    dry-run
        Exit before the run step -- this may still make network connections during workload initialization

    evaluate
        Print the evaluated YAML workload file with minimal validation

    list-actors
        List all actors available for use
    )";

    progDescription.add_options()
            ("help,h",
             "Show help message")
            ("run-mode", po::value<std::string>()->default_value("normal"),
             runModeHelp)
            ("list-actors",
                    "DEPRECATED. Please use the \"list-actors\" run mode.")
            ("dry-run",
                    "DEPRECATED. Please the \"dry-run\" run mode.")
            ("metrics-format,m",
             po::value<std::string>()->default_value("csv"),
             "Metrics format to use")
            ("metrics-output-file,o",
             po::value<std::string>()->default_value("/dev/stdout"),
             "Save metrics data to this file. Use `-` or `/dev/stdout` for stdout.")
            ("workload-file,w",
             po::value<std::string>(),
             "Path to workload configuration yaml file. "
             "Paths are relative to the program's cwd. "
             "Can also specify as the last positional argument.")
            ("mongo-uri,u",
             po::value<std::string>()->default_value("mongodb://localhost:27017"),
             "Mongo URI to use for the default connection-pool.");

    positional.add("workload-file", -1);

    auto run = po::command_line_parser(argc, argv)
            .options(progDescription)
            .positional(positional)
            .run();
    // clang-format on

    {
        auto stream = std::ostringstream();
        stream << progDescription;
        this->description = stream.str();
    }

    po::variables_map vm;
    po::store(run, vm);
    po::notify(vm);

    const auto runModeStr = vm["run-mode"].as<std::string>();

    if (vm.count("list-actors") >= 1 || runModeStr == "list-actors")
        this->runMode = RunMode::kListActors;

    if (vm.count("dry-run") >= 1 || runModeStr == "dry-run")
        this->runMode = RunMode::kDryRun;

    if (runModeStr == "evaluate")
        this->runMode = RunMode::kEvaluate;

    std::cout << runModeStr << "\n";


    this->isHelp = vm.count("help") >= 1;
    this->metricsFormat = vm["metrics-format"].as<std::string>();
    this->metricsOutputFileName = normalizeOutputFile(vm["metrics-output-file"].as<std::string>());
    this->mongoUri = vm["mongo-uri"].as<std::string>();

    if (vm.count("workload-file") > 0) {
        this->workloadSource = vm["workload-file"].as<std::string>();
        this->workloadSourceType = YamlSource::kFile;
    } else {
        this->workloadSourceType = YamlSource::kString;
    }
}
}  // namespace genny::driver