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

#ifndef HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED
#define HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED

namespace genny::canaries {

enum class WorkloadType {
    kNoop,
    kDbPing,
};

template <WorkloadType WType>
inline void doWork();

template <>
inline void doWork<WorkloadType::kNoop>() {
    int x = 0;
    // Ensure memory is flushed and instruct the compiler
    // to not optimize this line out.
    asm volatile("" : : "r,m"(x++) : "memory");
}

template <>
inline void doWork<WorkloadType::kDbPing>() {}

}  // namespace genny::canaries

#endif  // HEADER_521F5952_C79A_407E_9348_661B514D288D_INCLUDED