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
#include <map>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/filesystem.hpp>
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

namespace fs = boost::filesystem;

using YamlParameters = std::map<std::string, YAML::Node>;

YAML::Node recursiveParse(YAML::Node node, YamlParameters& params, const fs::path& phaseConfigPath);

YAML::Node loadConfig(const std::string& source,
                      DefaultDriver::ProgramOptions::YamlSource sourceType =
                          DefaultDriver::ProgramOptions::YamlSource::kFile) {

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

YAML::Node parseExternal(YAML::Node external, YamlParameters& params, const fs::path& phaseConfig) {
    int keysSeen = 0;

    if (!external["Path"]) {
        throw InvalidConfigurationException(
            "Missing the `Path` to-level key in your external phase configuration");
    }
    fs::path path(external["Path"].as<std::string>());
    keysSeen++;

    path = fs::absolute(phaseConfig / path);

    if (!fs::is_regular_file(path)) {
        auto os = std::ostringstream();
        os << "Invalid path to external PhaseConfig: " << path
           << ". Please ensure your workload file is placed in 'workloads/[subdirectory]/' and the "
              "'Path' parameter is relative to the 'phases/' directory";
        throw InvalidConfigurationException(os.str());
    }

    auto replacement = loadConfig(path.string());

    // Block of code for parsing the schema version.
    {
        if (!replacement["PhaseSchemaVersion"]) {
            throw InvalidConfigurationException(
                "Missing the `PhaseSchemaVersion` top-level key in your external phase "
                "configuration");
        }
        auto phaseSchemaVersion = replacement["PhaseSchemaVersion"].as<std::string>();
        if (phaseSchemaVersion != "2018-07-01") {
            auto os = std::ostringstream();
            os << "Invalid phase schema version: " << phaseSchemaVersion
               << ". Please ensure the schema for your external phase config is valid and the "
                  "`PhaseSchemaVersion` top-level key is set correctly";
            throw InvalidConfigurationException(os.str());
        }

        // Delete the schema version instead of adding it to `keysSeen`.
        replacement.remove("PhaseSchemaVersion");
    }

    if (external["Parameters"]) {
        keysSeen++;
        auto newParams = external["Parameters"].as<YamlParameters>();
        params.insert(newParams.begin(), newParams.end());
    }

    if (external["Key"]) {
        keysSeen++;
        const auto key = external["Key"].as<std::string>();
        if (!replacement[key]) {
            auto os = std::ostringstream();
            os << "Could not find top-level key: " << key << " in phase config YAML file: " << path;
            throw InvalidConfigurationException(os.str());
        }
        replacement = replacement[key];
    }

    if (external.size() != keysSeen) {
        auto os = std::ostringstream();
        os << "Invalid keys for 'External'. Please set 'Path' and if any, 'Parameters' in the YAML "
              "file: "
           << path << " with the following content: " << YAML::Dump(external);
        throw InvalidConfigurationException(os.str());
    }

    return recursiveParse(replacement, params, phaseConfig);
}

YAML::Node replaceParam(YAML::Node input, YamlParameters& params) {
    if (!input["Name"] || !input["Default"]) {
        auto os = std::ostringstream();
        os << "Invalid keys for '^Parameter', please set 'Name' and 'Default' in following node"
           << YAML::Dump(input);
        throw InvalidConfigurationException(os.str());
    }

    auto name = input["Name"].as<std::string>();
    // The default value is mandatory.
    auto defaultVal = input["Default"];

    // Nested params are ignored for simplicity.
    if (auto paramVal = params.find(name); paramVal != params.end()) {
        return paramVal->second;
    } else {
        return input["Default"];
    }
}

YAML::Node recursiveParse(YAML::Node node, YamlParameters& params, const fs::path& phaseConfig) {
    YAML::Node out;
    switch (node.Type()) {
        case YAML::NodeType::Map: {
            for (auto kvp : node) {
                if (kvp.first.as<std::string>() == "^Parameter") {
                    out = replaceParam(kvp.second, params);
                } else if (kvp.first.as<std::string>() == "ExternalPhaseConfig") {
                    auto external = parseExternal(kvp.second, params, phaseConfig);
                    // Merge the external node with the any other parameters specified
                    // for this node like "Repeat" or "Duration".
                    for (auto externalKvp : external) {
                        if (!out[externalKvp.first])
                            out[externalKvp.first] = externalKvp.second;
                    }
                } else {
                    out[kvp.first] = recursiveParse(kvp.second, params, phaseConfig);
                }
            }
            break;
        }
        case YAML::NodeType::Sequence: {
            for (auto val : node) {
                out.push_back(recursiveParse(val, params, phaseConfig));
            }
            break;
        }
        default:
            return node;
    }
    return out;
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

    if (options.workloadSource.empty()) {
        std::cerr << "Must specify a workload YAML file" << std::endl;
        setupCtx.failure();
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

    YamlParameters params;
    auto config = loadConfig(options.workloadSource, options.workloadSourceType);
    auto yaml = recursiveParse(config, params, phaseConfigSource);
    auto orchestrator = Orchestrator{};

    if (options.runMode == DefaultDriver::RunMode::kEvaluate) {
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

    {
        std::ofstream metricsOutput;
        metricsOutput.open(options.metricsOutputFileName,
                           std::ofstream::out | std::ofstream::trunc);
        reporter.report(metricsOutput, options.metricsFormat);
    }

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

    std::ostringstream progDescStream;
    // Section headers are prefaced with new lines.
    progDescStream << u8"\n🧞 Usage:\n";
    progDescStream << "    " << argv[0] << " <subcommand> [options] <workload-file>\n";
    progDescStream << u8"\n🧞 Subcommands:‍";
    progDescStream << u8R"(
    run          Run the workload normally
    dry-run      Exit before the run step -- this may still make network
                 connections during workload initialization
    evaluate     Print the evaluated YAML workload file with minimal validation
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
        return;
    }
    const auto subcommand = vm["subcommand"].as<std::string>();

    if (subcommand == "list-actors")
        this->runMode = RunMode::kListActors;
    else if (subcommand == "dry-run")
        this->runMode = RunMode::kDryRun;
    else if (subcommand == "evaluate")
        this->runMode = RunMode::kEvaluate;
    else if (subcommand == "run")
        this->runMode = RunMode::kNormal;
    else if (subcommand == "help")
        this->runMode = RunMode::kHelp;
    else {
        std::cerr << "ERROR: Unexpected subcommand " << subcommand << std::endl;
        this->runMode = RunMode::kHelp;
        return;
    }

    if (vm.count("help") >= 1)
        this->runMode = RunMode::kHelp;
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
