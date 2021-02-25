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

#include <testlib/findRepoRoot.hpp>

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <sstream>

namespace genny {

namespace {

constexpr auto ROOT_FILE = ".genny-root";

}  // namespace


//
// Note this function doesn't have any automated testing. Be careful when changing.
//
std::string findRepoRoot() {
    using namespace boost::filesystem;
    // NB for below:
    // canonical resolves foo/../bar to bar; it requires an absolute path.


    // Check for the GENNY_REPO_ROOT var set by the 'run-genny' wrapper.
    if (const auto rootEnvVar = getenv("GENNY_REPO_ROOT")) {
        if (!exists(path(rootEnvVar) / ROOT_FILE)) {
            std::stringstream msg;
            msg << "Cannot find '" << ROOT_FILE << "' in path from  GENNY_REPO_ROOT env var '"
                << rootEnvVar << "'";
            throw std::invalid_argument(msg.str());
        }
        return rootEnvVar;
    }


    {
        // Check for conventional workspace which has cwd=. and ./src/{repo} for all repos.
        auto conventionalPath = path("./src/genny/");
        if (exists(conventionalPath / ROOT_FILE)) {
            return conventionalPath.string();
        }
    }


    // Otherwise assume we're somewhere in ....../genny/some/subdir/of/the/genny/repo
    // and look up the directory tree for the .genny-repo-root file.
    const auto fileSystemRoot = canonical(absolute(path("/")));

    auto curr = current_path();
    const auto start = curr;

    while (!exists(curr / ROOT_FILE)) {
        curr = canonical(absolute(curr / "..", fileSystemRoot));
        if (curr == fileSystemRoot) {
            std::stringstream msg;
            msg << "Cannot find '" << ROOT_FILE << "' in '" << start << "'";
            throw std::invalid_argument(msg.str());
        }
    }

    return curr.string();
}


}  // namespace genny
