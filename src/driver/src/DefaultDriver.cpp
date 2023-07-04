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
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/stacktrace.hpp>

#include <yaml-cpp/yaml.h>

#include <loki/ScopeGuard.h>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/parallel.hpp>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

#include <driver/v1/DefaultDriver.hpp>

namespace genny::driver {
namespace {


YAML::Node loadFile(const std::string& source) {
    try {
        return YAML::LoadFile(source);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << source << ": " << ex.what();
        throw;
    }
}

namespace fs = boost::filesystem;

template <typename Actor>
void runActor(Actor&& actor,
              std::atomic<driver::DefaultDriver::OutcomeCode>& outcomeCode,
              Orchestrator& orchestrator) {
    auto guard = Loki::MakeGuard([&]() { orchestrator.abort(); });

    try {
        actor->run();
    } catch (const boost::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected boost::exception: "
                                 << boost::diagnostic_information(x, true)
                                 << boost::stacktrace::stacktrace();
        outcomeCode = driver::DefaultDriver::OutcomeCode::kBoostException;
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected std::exception: " << x.what() << std::endl
                                 << boost::diagnostic_information(x, true)
                                 << boost::stacktrace::stacktrace();
        outcomeCode = driver::DefaultDriver::OutcomeCode::kStandardException;
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Unknown error";
        // Don't try to handle unknown errors, let us crash ungracefully
        throw;
    }
}

void reportMetrics(genny::metrics::Registry& metrics,
                   const std::string& actorName,
                   const std::string& operationName,
                   bool success,
                   metrics::clock::time_point startTime) {
    auto finishTime = metrics::clock::now();
    auto actorSetup = metrics.operation(actorName, operationName, 0u, std::nullopt, true);
    auto outcome = success ? metrics::OutcomeType::kSuccess : metrics::OutcomeType::kFailure;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(finishTime - startTime);
    actorSetup.report(std::move(finishTime), std::move(duration), std::move(outcome));
}

/**
 * Filter to remove any nodes inside dedicated "ignore" values.
 *
 * Basically re-implementing the following python logic:
 *  ignored = set("foo","bar")
 *  return [u for u in unused if not any(ignore in u for ignore in ignored)]
 */
UnusedNodes removeIgnored(const UnusedNodes& unused, const std::vector<std::string>& ignored) {
    UnusedNodes out{};
    std::copy_if(
        unused.begin(), unused.end(), std::back_inserter(out), [&](const std::string& path) {
            return !std::any_of(ignored.begin(), ignored.end(), [&](const std::string& ignore) {
                return path.find(ignore) != std::string::npos;
            });
        }
    );
    return out;
}

/**
 * Log a message showing the unused yaml structures.
 */
void reportUnused(const NodeSource& nodeSource, const bool dryrun) {
    auto raw = nodeSource.unused();

    std::vector<std::string> ignored{".yml/Description", ".yml/Owner", ".yml/AutoRun", ".yml/Keywords", ".yml/Clients"};
    if (dryrun) {
        ignored.push_back(".yml/Clients");
    }

    auto unused = removeIgnored(raw, ignored);
    auto many = std::distance(unused.begin(), unused.end());
    auto verb = many == 1 ? "was" : "were";
    auto plural = many == 1 ? "" : "s";
    auto action = dryrun ? "constructing" : "running";
    std::stringstream message;
    message << std::endl << "<BETA FEATURE> - YAML Usage Check" << std::endl;
    
    if (!unused.empty()) {
        message << "There " << verb << " " << many << " YAML structure" << plural
                            << " unused when " << action << " the workload." << std::endl;
        message << "\n\t" << boost::algorithm::join(unused, "\n\t") << std::endl;
        
    } else {
        message << "All YAML structures appear to have been used when " << action
                << " this workload." << std::endl;
    }
    message << "The following nodes were ignored in this analysis:" << std::endl;
    message << "\n\t" << boost::algorithm::join(ignored, "\n\t") << std::endl;

    message << "Incorrect results are possible. "
            << "Please file a TIG ticket on the TIPS backlog, or otherwise let us know in the "
            << "#performance-tooling-users slack channel if this looks wrong." << std::endl;
    message << "</BETA FEATURE>";
    if (unused.empty()) {
        BOOST_LOG_TRIVIAL(info) << message.str();
    } else {
        BOOST_LOG_TRIVIAL(warning) << message.str();
    }
}

DefaultDriver::OutcomeCode doRunLogic(const DefaultDriver::ProgramOptions& options) {
    // setup logging as the first thing we do.
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= options.logVerbosity);

    const auto workloadName = fs::path(options.workloadSource).stem().string();
    auto startTime = genny::metrics::Registry::clock::now();

    if (options.runMode == DefaultDriver::RunMode::kListActors) {
        globalCast().streamProducersTo(std::cout);
        genny::metrics::Registry metrics;
        reportMetrics(metrics, workloadName, "Setup", true, startTime);
        return DefaultDriver::OutcomeCode::kSuccess;
    }

    if (options.workloadSource.empty()) {
        std::cerr << "Must specify a workload YAML file" << std::endl;
        genny::metrics::Registry metrics;
        reportMetrics(metrics, workloadName, "Setup", false, startTime);
        return DefaultDriver::OutcomeCode::kUserException;
    }

    fs::path phaseConfigSource;
    if (options.workloadSourceType == DefaultDriver::ProgramOptions::YamlSource::kString) {
        phaseConfigSource = fs::current_path();
    } else {
        /*
         * The directory structure for workloads and phase configs is as follows:
         * /etc (or /src if building without installing)
         *     /workloads
         *         /[name-of-workload-theme]
         *             /[my-workload.yml]
         * The path to the phase config snippet is obtained as a relative path from the workload
         * file.
         */
        phaseConfigSource = fs::path(options.workloadSource).parent_path();
    }

    YAML::Node yaml;
    if (options.workloadSourceType == DefaultDriver::ProgramOptions::YamlSource::kFile) {
        yaml = loadFile(options.workloadSource);
    } else if (options.workloadSourceType == DefaultDriver::ProgramOptions::YamlSource::kString) {
        yaml = YAML::Load(options.workloadSource);
    } else {
        throw std::invalid_argument("Unrecognized workload source type.");
    }

    auto orchestrator = Orchestrator{};

    NodeSource nodeSource{YAML::Dump(yaml),
                          options.workloadSourceType ==
                                  DefaultDriver::ProgramOptions::YamlSource::kFile
                              ? options.workloadSource
                              : "inline-yaml"};


    auto workloadContext = WorkloadContext{nodeSource.root(),
                                           orchestrator,
                                           globalCast(),
                                           {},
                                           options.runMode == DefaultDriver::RunMode::kDryRun};

    genny::metrics::Registry& metrics = workloadContext.getMetrics();

    if (options.runMode == DefaultDriver::RunMode::kDryRun) {
        BOOST_LOG_TRIVIAL(info) << "Workload context constructed without errors.";
        reportMetrics(metrics, workloadName, "Setup", true, startTime);
        for (const auto& [phaseNum, phaseContexts] : workloadContext.getActivePhaseContexts()) {
            BOOST_LOG_TRIVIAL(debug) << "Phase " << phaseNum << " Actors:";
            int i = 0;
            for (const PhaseContext& phaseCtx : phaseContexts) {
                const auto& actorCtx = phaseCtx.actor();
                BOOST_LOG_TRIVIAL(debug) << ++i << ") " << actorCtx.getType() << "." << actorCtx.getName();
            }
        }
        
        reportUnused(nodeSource, true);
        return DefaultDriver::OutcomeCode::kSuccess;
    }

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));

    reportMetrics(metrics, workloadName, "Setup", true, startTime);

    auto startedActors = metrics.operation(workloadName, "ActorStarted", 0u, std::nullopt, true);
    auto finishedActors = metrics.operation(workloadName, "ActorFinished", 0u, std::nullopt, true);

    std::atomic<DefaultDriver::OutcomeCode> outcomeCode = DefaultDriver::OutcomeCode::kSuccess;

    std::mutex reporting;
    parallelRun(workloadContext.actors(),
                [&](const auto& actor) {
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
               });

    if (metrics.getFormat().useCsv()) {
        const auto reporter = genny::metrics::Reporter{metrics};

        {
            std::ofstream metricsOutput;
            metricsOutput.open(metrics.getPathPrefix().string() + ".csv",
                               std::ofstream::out | std::ofstream::trunc);
            reporter.report(metricsOutput, metrics.getFormat());
        }
    }

    // We don't use the workload name because downstream sources may expect consistent
    // names for timing files.
    reportMetrics(metrics, "WorkloadTimingRecorder", "Workload", true, startTime);

    reportUnused(nodeSource, false);
    return outcomeCode;
}

}  // namespace


DefaultDriver::OutcomeCode DefaultDriver::run(const DefaultDriver::ProgramOptions& options) const {
    try {
        // Wrap doRunLogic in another catch block in case it throws an exception of its own e.g.
        // file not found or io errors etc - exceptions not thrown by ActorProducers.
        return doRunLogic(options);
    } catch (const boost::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Caught boost::exception "
                                 << boost::diagnostic_information(x, true)
                                 << boost::stacktrace::stacktrace();
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Caught std::exception " << x.what() << std::endl
                                 << boost::diagnostic_information(x, true)
                                 << boost::stacktrace::stacktrace();;
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

boost::log::trivial::severity_level parseVerbosity(const std::string& level) {

    if (level == "trace") {
        return boost::log::trivial::trace;
    }
    if (level == "debug") {
        return boost::log::trivial::debug;
    }
    if (level == "info") {
        return boost::log::trivial::info;
    }
    if (level == "warning") {
        return boost::log::trivial::warning;
    }
    if (level == "error") {
        return boost::log::trivial::error;
    }
    if (level == "fatal") {
        return boost::log::trivial::fatal;
    }

    throw std::invalid_argument("Invalid verbosity level '" + level +
                                "'. Need one of trace/debug/info/warning/error/fatal");
}

}  // namespace

const std::string RUNNER_NAME = "genny";

DefaultDriver::ProgramOptions::ProgramOptions(int argc, char** argv) {
    namespace po = boost::program_options;

    std::ostringstream progDescStream;
    // Section headers are prefaced with new lines.
    progDescStream << u8"\n🧞 Usage:\n";
    progDescStream << "    " << RUNNER_NAME << " <subcommand> [options] <workload-file>\n";
    progDescStream << u8"\n🧞 Subcommands:‍";
    progDescStream << u8R"(
    run          Run the workload normally
    dry-run      Exit before the run step -- this may still make network
                 connections during workload initialization
    list-actors  List all actors available for use
    )" << "\n";

    progDescStream << "🧞 Options";
    po::options_description progDescription{progDescStream.str()};
    po::positional_options_description positional;

    // clang-format off
    progDescription.add_options()
            ("subcommand", po::value<std::string>(), "1st positional argument")
            ("help,h",
             "Show help message")

            ("workload-file,w",
             po::value<std::string>(),
             "Path to workload configuration yaml file. "
             "Paths are relative to the program's cwd. "
             "Can also specify as the last positional argument.")
            ("verbosity,v",
              po::value<std::string>()->default_value("info"),
              "Log severity for boost logging. Valid values are trace/debug/info/warning/error/fatal.");

    positional.add("subcommand", 1);
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

    if (!vm.count("subcommand")) {
        std::cerr << "ERROR: missing subcommand" << std::endl;
        this->runMode = RunMode::kHelp;
        this->parseOutcome = OutcomeCode::kUserException;
        return;
    }
    const auto subcommand = vm["subcommand"].as<std::string>();

    if (subcommand == "list-actors")
        this->runMode = RunMode::kListActors;
    else if (subcommand == "dry-run")
        this->runMode = RunMode::kDryRun;
    else if (subcommand == "run")
        this->runMode = RunMode::kNormal;
    else if (subcommand == "help")
        this->runMode = RunMode::kHelp;
    else {
        std::cerr << "ERROR: Unexpected subcommand " << subcommand << std::endl;
        this->runMode = RunMode::kHelp;
        this->parseOutcome = OutcomeCode::kUserException;
        return;
    }

    if (vm.count("help") >= 1)
        this->runMode = RunMode::kHelp;

    this->logVerbosity = parseVerbosity(vm["verbosity"].as<std::string>());

    if (vm.count("workload-file") > 0) {
        this->workloadSource = vm["workload-file"].as<std::string>();
        this->workloadSourceType = YamlSource::kFile;
    } else {
        this->workloadSourceType = YamlSource::kString;
    }
}
}  // namespace genny::driver
