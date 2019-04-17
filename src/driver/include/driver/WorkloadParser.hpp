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

#include <driver/DefaultDriver.hpp>

namespace genny::driver {

using YamlParameters = std::map<std::string, YAML::Node>;
namespace fs = boost::filesystem;

class WorkloadParser {

public:
    WorkloadParser() = default;

    YAML::Node parse(const std::string& source,
                     const fs::path& phaseConfig,
                     DefaultDriver::ProgramOptions::YamlSource =
                         DefaultDriver::ProgramOptions::YamlSource::kFile);

private:
    YamlParameters _params;

    YAML::Node recursiveParse(YAML::Node, const fs::path&);
    YAML::Node loadFile(const std::string& path);
    YAML::Node parseExternal(YAML::Node, const fs::path&);
    YAML::Node replaceParam(YAML::Node);
};

}  // namespace genny::driver


#endif  // HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED
