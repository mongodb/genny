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

namespace {

using namespace genny;
using namespace genny::driver;
namespace fs = boost::filesystem;

using YamlParameters = std::map<std::string, YAML::Node>;

YAML::Node recursiveParse(const YAML::Node& node,
                          YamlParameters& params,
                          const fs::path& phaseConfigPath);

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

YAML::Node parseExternal(const YAML::Node& external,
                         YamlParameters& params,
                         const fs::path& phaseConfig) {
    int keysSeen = 1;

    fs::path path(external["Path"].as<std::string>());
    path = fs::absolute(phaseConfig / path);

    if (!fs::is_regular_file(path)) {
        auto os = std::ostringstream();
        os << "Invalid path to external PhaseConfig: " << path
           << ". Please ensure your workload file is placed in 'workloads/[subdirectory]/' and the "
              "'Path' parameter is relative to the 'phase_configs/' directory";
        throw InvalidConfigurationException(os.str());
    }

    auto replacement = loadConfig(path.string());

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

YAML::Node replaceParam(const YAML::Node& input, YamlParameters& params) {
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

YAML::Node recursiveParse(const YAML::Node& node,
                          YamlParameters& params,
                          const fs::path& phaseConfig) {
    YAML::Node out;
    switch (node.Type()) {
        case YAML::NodeType::Map: {
            for (auto&& kvp : node) {
                if (kvp.first.as<std::string>() == "^Parameter") {
                    out = replaceParam(kvp.second, params);
                } else if (kvp.first.as<std::string>() == "ExternalPhaseConfig") {
                    auto external = parseExternal(kvp.second, params, phaseConfig);
                    // Merge the external node with the any other parameters specified
                    // for this node like "Repeat" or "Duration".
                    for (auto&& kvp : external) {
                        out[kvp.first] = kvp.second;
                    }
                } else {
                    out[kvp.first] = recursiveParse(kvp.second, params, phaseConfig);
                }
            }
            break;
        }
        case YAML::NodeType::Sequence: {
            for (auto&& val : node) {
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

genny::driver::DefaultDriver::OutcomeCode doRunLogic(
    const genny::driver::DefaultDriver::ProgramOptions& options) {
    if (options.shouldListActors) {
        globalCast().streamProducersTo(std::cout);
        return genny::driver::DefaultDriver::OutcomeCode::kSuccess;
    }

    genny::metrics::Registry metrics;

    auto actorSetup = metrics.operation("Genny", "Setup", 0u);
    auto setupCtx = actorSetup.start();

    auto config = loadConfig(options.workloadSource, options.workloadSourceType);
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
         *     /phase_configs
         *         /[name-of-actor]
         *             /[name-of-phase-config-snippet.yml]
         *
         * The path to the phase config snippet is obtained as a relative path from the workload
         * file.
         */
        phaseConfigSource =
            fs::path(options.workloadSource).parent_path().parent_path().parent_path() /
            "phase_configs";
    }
    YamlParameters params;
    auto yaml = recursiveParse(config, params, phaseConfigSource);

    auto orchestrator = Orchestrator{};

    auto workloadContext =
        WorkloadContext{yaml, metrics, orchestrator, options.mongoUri, globalCast()};

    if (options.isDryRun) {
        std::cout << "Workload context constructed without errors." << std::endl;
        return genny::driver::DefaultDriver::OutcomeCode::kSuccess;
    }

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));

    setupCtx.success();

    auto startedActors = metrics.operation("Genny", "ActorStarted", 0u);
    auto finishedActors = metrics.operation("Genny", "ActorFinished", 0u);

    std::atomic<driver::DefaultDriver::OutcomeCode> outcomeCode =
        driver::DefaultDriver::OutcomeCode::kSuccess;

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


genny::driver::DefaultDriver::OutcomeCode genny::driver::DefaultDriver::run(
    const genny::driver::DefaultDriver::ProgramOptions& options) const {
    try {
        // Wrap doRunLogic in another catch block in case it throws an exception of its own e.g.
        // file not found or io errors etc - exceptions not thrown by ActorProducers.
        return doRunLogic(options);
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Caught exception " << x.what();
    }
    return genny::driver::DefaultDriver::OutcomeCode::kInternalException;
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


genny::driver::DefaultDriver::ProgramOptions::ProgramOptions(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description description{u8"🧞‍ Allowed Options 🧞‍"};
    po::positional_options_description positional;

    // clang-format off
    description.add_options()
        ("help,h",
            "Show help message")
        ("list-actors",
            "List all actors available for use")
        ("dry-run",
            "Exit before the run step---"
            "this may still make network connections during workload initialization")
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
            "Can also specify as first positional argument.")
        ("mongo-uri,u",
            po::value<std::string>()->default_value("mongodb://localhost:27017"),
            "Mongo URI to use for the default connection-pool.")
    ;

    positional.add("workload-file", -1);

    auto run = po::command_line_parser(argc, argv)
        .options(description)
        .positional(positional)
        .run();
    // clang-format on

    {
        auto stream = std::ostringstream();
        stream << description;
        this->description = stream.str();
    }

    po::variables_map vm;
    po::store(run, vm);
    po::notify(vm);

    this->isHelp = vm.count("help") >= 1;
    this->shouldListActors = vm.count("list-actors") >= 1;
    this->isDryRun = vm.count("dry-run") >= 1;
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
