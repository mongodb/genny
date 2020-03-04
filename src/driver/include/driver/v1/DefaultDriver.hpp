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

#ifndef HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
#define HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED

#include <optional>
#include <string>
#include <vector>

#include <boost/log/trivial.hpp>

#include <gennylib/ActorProducer.hpp>
#include <gennylib/ActorVector.hpp>
#include <metrics/metrics.hpp>

namespace genny::driver {

/**
 * Basic workload driver that spins up one thread per actor.
 */
class DefaultDriver {
public:
    enum class RunMode {
        kNormal,
        kDryRun,
        kEvaluate,
        kListActors,
        kHelp,
    };

    enum class OutcomeCode {
        kSuccess = 0,
        kStandardException = 1,
        kBoostException = 2,
        kInternalException = 3,
        kUserException = 4,
        kUnknownException = 10,
    };

    
    struct ProgramOptions {
        explicit ProgramOptions() = default;

        /**
         * @param argc c-style argc
         * @param argv c-style argv
         */
        ProgramOptions(int argc, char** argv);

        enum class YamlSource { kFile, kString };
        YamlSource workloadSourceType = YamlSource::kFile;
        std::string workloadSource;  // either file name or yaml

        metrics::MetricsFormat metricsFormat;
        std::string metricsOutputFileName;
        std::string metricsPathPrefix;
        std::string mongoUri;
        std::string description;
        bool isSmokeTest;
        DefaultDriver::RunMode runMode = RunMode::kNormal;
        boost::log::trivial::severity_level logVerbosity;
    };

    /**
     * @return c-style exit code
     */
    OutcomeCode run(const ProgramOptions& options) const;
};

}  // namespace genny::driver

#endif  // HEADER_81A374DA_8E23_4E4D_96D2_619F27016F2A_INCLUDED
