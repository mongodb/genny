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

#include <gennylib/ExecutionStrategy.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/context.hpp>

namespace genny {

void ExecutionStrategy::_finishRun(const RunOptions& options, Result result) {
    if (!result.wasSuccessful) {
        BOOST_LOG_TRIVIAL(error) << "Operation failed after " << options.maxRetries
                                 << " retry attempts.";
    }

    _lastResult = std::move(result);
}
}  // namespace genny
