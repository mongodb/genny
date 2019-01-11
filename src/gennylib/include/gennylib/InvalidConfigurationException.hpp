// Copyright 2018 MongoDB Inc.
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

#ifndef HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
#define HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED

#include <exception>
#include <stdexcept>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidConfigurationException : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

}  // namespace genny


#endif  // HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
