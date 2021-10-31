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

#include <boost/throw_exception.hpp>
#include <boost/exception/exception.hpp>
#include <boost/log/trivial.hpp>

#include <algorithm>
#include <iterator>
#include <variant>
#include <thread>
#include <vector>
#include <deque>

namespace genny {

    template<typename IterableT, typename BinaryOperation>
    void parallelRun(IterableT& iterable, BinaryOperation op) {
        auto threadsPtr = std::make_unique<std::vector<std::thread>>();
        threadsPtr->reserve(std::distance(cbegin(iterable), cend(iterable)));
        std::deque<std::exception_ptr> caughtExceptions;
        std::mutex exceptLock;
        std::transform(cbegin(iterable), cend(iterable), std::back_inserter(*threadsPtr),
                [&](const typename IterableT::value_type& value) {
                    return std::thread{[&]() {
                        try {
                            op(value);
                        } catch(...) {
                            const std::lock_guard<std::mutex> lock(exceptLock);
                            caughtExceptions.push_back(std::current_exception());
                        }
                    }};
                });

        for (auto& thread : *threadsPtr) {
            thread.join();
        }
        for (auto&& exc : caughtExceptions) {
            std::rethrow_exception(exc);
        }
    }

} // namespace genny::v1

#endif // HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
