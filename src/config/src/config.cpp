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

#include <config/config.hpp>

namespace genny {

YAML::Node Node::parse(std::string yaml) {
    // TODO: better error message if this fails
    return YAML::Load(yaml);
}

std::string Node::path() const {
    std::ostringstream out;
    this->appendKey(out);
    return out.str();
}

}