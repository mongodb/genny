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

#include <iostream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include <canaries/Loops.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/context.hpp>

using namespace genny;

struct ProgramOptions {
    std::vector<std::string> _testNames;
    std::string _description;
    bool _isHelp = false;
    bool _withPing = false;
    int64_t _iterations = 0;

    explicit ProgramOptions() = default;

    ProgramOptions(int argc, char** argv) {
        namespace po = boost::program_options;

        std::ostringstream progDesc;
        progDesc << "Genny Canaries - Microbenchmarks for measuring overhead of various Genny "
                    "functionality by running various low-level workloads with Genny\n\n";
        progDesc << "Usage:\n";
        progDesc << "    " << argv[0] << " work-name <loop-type [loop-type] ...>\n\n";
        progDesc << "Workloads:‍";
        progDesc << u8R"(
    noop    Trivial workload that reads a value from a register; intended for testing loops
            with the minimum amount of unrelated code. Note that fully "noop" is not possible
            as it will cause the entire loop to be optimized out.
    sleep   Sleep for a 100ms.
    cpu     Multiply a large number 10000 times to stress the CPU's ALU.
    l2      Traverse through a 2MB array in 64KB strides. This stresses the CPU's L2 cache.
    l3      Traverse through a 8MB array in 64KB strides. This stresses the CPU's L3 cache
            and/or RAM depending the CPU and its load.
    ping    call db.ping() on a MongoDB server (running externally)
    )" << "\n";

        progDesc << "Options:";
        po::options_description optDesc(progDesc.str());
        po::positional_options_description positional;

        // clang-format off
        optDesc.add_options()
        ("help,h",
                "Show this help message")
        ("test-name",
                po::value<std::vector<std::string>>()->multitoken(),
                "One or more names of tests to run")
        ("run-ping,p",
                po::value<bool>()->default_value(true),
                "Whether to run a db.ping() in each iteration of the canary or do nothing."
                "Running db.ping() will simulate real-world performance more closely while"
                "running nothing may help profile genny code with less noise")
        ("iterations,i",
                po::value<int64_t>()->default_value(1e6),
                "number of iterations to run the tests");
        //clang-format on

        positional.add("test-name", -1);

        auto run =
            po::command_line_parser(argc, argv).options(optDesc).positional(positional).run();

        std::ostringstream stm;
        stm << optDesc;
        _description = stm.str();

        po::variables_map vm;
        po::store(run, vm);
        po::notify(vm);

        if (vm.count("help") >= 1)
            _isHelp = true;

        _withPing = vm["run-ping"].as<bool>();
        _iterations = vm["iterations"].as<int64_t>();

        if (vm.count("test-name") >= 1)
            _testNames = vm["test-name"].as<std::vector<std::string>>();
    }
};

template <genny::canaries::WorkloadType WType>
void runTest(std::vector<std::string>& testNames, int64_t iterations) {
    canaries::Loops<WType> loops(iterations);
    for (const auto& testName : testNames) {
        int64_t time;

        // Run each test twice, the first time is warm up and the results are discarded.
        if (testName == "simple-loop") {
            loops.simpleLoop();
            time = loops.simpleLoop();
        } else if (testName == "phase-loop") {
            loops.simpleLoop();
            time = loops.phaseLoop();
        } else if (testName == "metrics-loop") {
            loops.simpleLoop();
            time = loops.metricsLoop();
        } else if (testName == "real-loop") {
            loops.simpleLoop();
            time = loops.metricsPhaseLoop();
        } else {
            std::ostringstream stm;
            stm << "Unknown test name: " << testName;
            throw InvalidConfigurationException(stm.str());
        }
        std::cout << testName << ":\t" << time << "ns" << std::endl;
    }

}

int main(int argc, char** argv) {
    using genny::canaries::WorkloadType;
    auto opts = ProgramOptions(argc, argv);
    if (opts._isHelp || opts._testNames.empty()) {
        std::cout << opts._description << std::endl;
        return 0;
    }

    if (opts._withPing)
        runTest<WorkloadType::kDbPing>(opts._testNames, opts._iterations);
    else
        runTest<WorkloadType::kNoop>(opts._testNames, opts._iterations);

    return 0;
}
