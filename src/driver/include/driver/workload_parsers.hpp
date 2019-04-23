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

#ifndef HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED
#define HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED

#include <boost/filesystem.hpp>
#include <yaml-cpp/yaml.h>

#include <driver/v1/DefaultDriver.hpp>

namespace genny::driver::v1 {

using YamlParameters = std::map<std::string, YAML::Node>;
namespace fs = boost::filesystem;

/**
 * Parse user-defined workload files into shapes suitable for Genny.
 */
class WorkloadParser {
public:
    // Whether to parse the workload normally or for smoke test.
    enum class Mode { kNormal, kSmokeTest };

    explicit WorkloadParser(fs::path phaseConfigPath)
        : _phaseConfigPath{std::move(phaseConfigPath)} {};

    YAML::Node parse(const std::string& source,
                     DefaultDriver::ProgramOptions::YamlSource =
                         DefaultDriver::ProgramOptions::YamlSource::kFile,
                     Mode mode = Mode::kNormal);

private:
    YamlParameters _params;
    const fs::path _phaseConfigPath;

    YAML::Node recursiveParse(YAML::Node);

    // The following group of methods handle workload files containing external configuration files.
    void convertExternal(std::string key, YAML::Node value, YAML::Node& out);
    YAML::Node parseExternal(YAML::Node);
    YAML::Node replaceParam(YAML::Node);
};

/**
 * Convert a workload YAML into a version for smoke test where every phase
 * of every actor runs with Repeat: 1
 */
class SmokeTestConverter {
public:
    static YAML::Node convert(YAML::Node workloadRoot);
};

}  // namespace genny::driver::v1


#endif  // HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED
