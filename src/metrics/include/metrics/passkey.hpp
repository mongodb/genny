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

#ifndef HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED
#define HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED

#include <cassert>

namespace genny::metrics {
namespace v1 {

/**
 * The ReporterT is given read-access to metrics data for the purposes
 * of reporting data. The ReporterT class is the only separately-compiled
 * component of the metrics library. It is not ABI safe.
 */
template <typename MetricsClockSource>
class ReporterT;


/**
 * Ignore this. Used for passkey for some methods.
 */
class Evil {
protected:
    Evil() = default;
};


/**
 * TODO: Add a description of the passkey idiom.
 *
 * See also https://arne-mertz.de/2016/10/passkey-idiom/.
 */
class Permission : private Evil {
private:
    constexpr Permission() = default;

    template <typename MetricsClockSource>
    friend class ReporterT;
};

static_assert(std::is_empty<Permission>::value, "empty");

}  // namespace v1
}  // namespace genny::metrics

#endif  // HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED
