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

#ifndef HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
#define HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED

#include <algorithm>
#include <iterator>
#include <thread>
#include <vector>

namespace genny {

    template<typename IteratorT, typename BinaryOperation>
    void parallelRun(IteratorT begin, IteratorT end, BinaryOperation op) {
        std::vector<std::thread> threads;
        std::transform(begin, end, std::back_inserter(threads), op);
        for (auto& thread : threads) {
            thread.join();
        }
    }

} // namespace genny::v1

#endif // HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
