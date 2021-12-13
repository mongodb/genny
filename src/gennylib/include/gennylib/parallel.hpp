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
#include <mutex>
#include <vector>
#include <deque>

namespace genny {

/**
 * Class that can collect items in a thread-safe way, and then allows "extracting" the
 * contents as a std::vector, usually in a single-threaded context after parallel execution.
 */
template <typename T>
class ParallelBucket {
public:
    ParallelBucket() = default;

    /**
     * Add an item to the bucket.
     */
    void addItem(T&& item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _items.push_back(std::move(item));
    }

    /**
     * Extract items from the bucket. This "resets" it.
     */
    std::vector<T> extractItems() {
        std::lock_guard<std::mutex> lock(_mutex);
        auto temp = std::move(_items);
        // Put this back into a valid state just in case someone wants to use it.
        _items = std::vector<T>();
        return std::move(temp);
    }
    
private:
    mutable std::mutex _mutex;
    std::vector<T> _items;
};

/*
 * Thread-safe collector for exceptions during parallel execution.
 */
class ExceptionBucket {
public:

    ExceptionBucket() = default;

    /**
     * Class that represents a bundle of exceptions thrown together after
     * ending parallel execution.
     */
    class ParallelException : public std::exception {
    public:

        ParallelException(std::vector<std::exception_ptr>&& exceptions) : _caughtExceptions{std::move(exceptions)} {
            if (_caughtExceptions.empty()) {
                std::logic_error("Tried to construct ParallelException, but no exceptions were given.");
            }
            std::stringstream ss;
            ss << "Error in parallel execution. First exception's what(): ";

            // We cannot just call what() through the exception pointer, unfortunately.
            try {
                std::rethrow_exception(_caughtExceptions[0]);
            } catch (const std::exception& e) {
                ss << e.what();
            } catch (...) {
                throw std::logic_error("ParallelException constructed with unknown exception.");
            }

            _message = ss.str();
        }

        /*
         * Get the exceptions held by this ParallelException.
         */
        std::vector<std::exception_ptr>& exceptions() {
            return _caughtExceptions;
        }

        const char* what() const noexcept override {
            return _message.c_str();
        }

    private:
        std::vector<std::exception_ptr> _caughtExceptions;
        std::string _message = "No exception? This shouldn't have been constructed and thrown.";
    };


    /**
     * Add an exception to the bucket.
     */
    void addException(std::exception_ptr&& exc) {
        _caughtExceptions.addItem(std::move(exc));
    }

    
    /**
     * If any exceptions were caught, throw them
     * as a ParallelException.
     */
    void throwIfExceptions() {
        auto excVec = _caughtExceptions.extractItems();
        if (!excVec.empty()) {
            BOOST_THROW_EXCEPTION(ParallelException(std::move(excVec)));
        }
    }

private:
    ParallelBucket<std::exception_ptr> _caughtExceptions;
};

/**
 * Given iterable, run op in one thread per element of iterable, passing
 * a reference to the element for each execution.
 *
 * Any exception thrown in any thread is gathered and rethrown in the calling thread.
 */
template<typename IterableT, typename BinaryOperation>
void parallelRun(IterableT& iterable, BinaryOperation op) {
    auto threadsPtr = std::make_unique<std::vector<std::thread>>();
    threadsPtr->reserve(std::distance(cbegin(iterable), cend(iterable)));
    ExceptionBucket caughtExc;
    std::transform(cbegin(iterable), cend(iterable), std::back_inserter(*threadsPtr),
            [&](const typename IterableT::value_type& value) {
                return std::thread{[&]() {
                    try {
                        op(value);
                    } catch(...) {
                        caughtExc.addException(std::move(std::current_exception()));
                    }
                }};
            });

    for (auto& thread : *threadsPtr) {
        thread.join();
    }
    caughtExc.throwIfExceptions();
}

} // namespace genny::v1

#endif // HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
