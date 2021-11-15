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
#include <boost/core/noncopyable.hpp>
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
 * Data structure that wraps STL containers and is thread-safe for insertions.
 *
 * Remain vigilant about the fact that this container offers no guarantees regarding
 * the thread-safety of underlying container elements after returning.
 *
 * This container object also meets the BasicLockable requirement.
 */
template <typename T>
class AtomicContainer {
public:   
    using value_type = typename T::value_type;

    AtomicContainer() = default;
    AtomicContainer(const AtomicContainer& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
        _assertNotForciblyHeld();
        this->_container = rhs._container;
    }
    AtomicContainer(AtomicContainer&& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
        _assertNotForciblyHeld();
        this->_container = std::move(rhs._container);
    }
    AtomicContainer<T>& operator=(const AtomicContainer& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
        _assertNotForciblyHeld();
        this->_container = rhs._container;
        return *this;
    }
    AtomicContainer<T>& operator=(AtomicContainer&& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
        this->_container = std::move(rhs._container);
    }

    // STL Methods
    auto&& front() {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.front();
    }
    template <typename... Args>
    auto&& emplace_back(Args... args) {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.emplace_back(std::forward<Args>(args)...);
    }
    void push_back(const typename T::value_type& value) {
        std::lock_guard<lock_t> lock(_mutex);
        _container.push_back(value);
    }
    void push_back(typename T::value_type&& value) {
        std::lock_guard<lock_t> lock(_mutex);
        _container.push_back(std::move(value));
    }
    auto&& back() {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.back();
    }

    void pop_front() {
        std::lock_guard<lock_t> lock(_mutex);
        _container.pop_front();
        return;
    }
    
    /*bool empty() const {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.empty();
    }*/

    const auto& operator[](size_t pos) const {
        std::lock_guard<lock_t> lock(_mutex);
        return _container[pos];
    }
    auto& operator[](size_t pos) {
        std::lock_guard<lock_t> lock(_mutex);
        return _container[pos];
    }

    /**
     * It's possibly abusive to be treating these as const, but we want to be able to do
     * locked iteration in const contexts, and we treat the locks as mutable elsewhere.
     */
    void lock() const {
        // Paranoidly hold the lock here in case there are shenanigans
        // between the flag flip and the unlock.
        std::lock_guard<lock_t> lock(_mutex);
        _assertNotForciblyHeld();
        _mutex.lock();
        _forciblyHeld = true;
    }
    void unlock() const {
        // Paranoidly hold the lock here in case there are shenanigans
        // between the flag flip and the unlock.
        std::lock_guard<lock_t> lock(_mutex);
        _forciblyHeld = false;
        _mutex.unlock();
    }

    size_t size() const {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.size();
    }

    /**
     * Returns iterators to the underlying container.
     *
     * Iterator methods require forcibly holding the underlying lock with
     * lock() and unlock(). Since this type satisfies the BasicLockable
     * requirement, use a std::lock_guard like so:
     *
     *      {
     *          std::lock_guard<AtomicContainerType> lock(containerVar);
     *          for (auto&& var : containerVar) {
     *              // Iterator stuff
     *          }
     *      }
     *
     * Note that it is physically possible for someone to create an iterator,
     * unlock the container, and then do unsafe iteration. Don't do that.
     * */
    typename T::iterator begin() {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.begin();
    }
    typename T::iterator end() {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.end();
    }

    typename T::const_iterator begin() const {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.begin();
    }
    typename T::const_iterator cbegin() const {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.cbegin();
    }
    typename T::const_iterator end() const {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.end();
    }
    typename T::const_iterator cend() const {
        std::lock_guard<lock_t> lock(_mutex);
        _assertForciblyHeld();
        return _container.cend();
    }

private:
    void _assertForciblyHeld() const {
        if (!_forciblyHeld) {
            std::stringstream ss; 
            ss << "Must forcibly hold container lock in order to iterate. Call container.lock() and"
               << " container.unlock() around iteration, preferrably using a std::lock_guard.";
            BOOST_THROW_EXCEPTION(
                std::logic_error(ss.str()));
        }
    }

    void _assertNotForciblyHeld() const {
        if (_forciblyHeld) {
            BOOST_THROW_EXCEPTION(
                std::logic_error("Cannot forcibly hold AtomicContainer lock multiple times."));
        }
    }
    using lock_t = std::recursive_mutex;
    mutable bool _forciblyHeld = false;
    T _container;
    mutable lock_t _mutex;
};

template <typename T>
using AtomicDeque = AtomicContainer<std::deque<T>>;

template <typename T>
using AtomicVector = AtomicContainer<std::vector<T>>;

class ParallelException : public std::exception {
public:

    ParallelException(std::deque<std::exception_ptr>&& exceptions) {
        _caughtExceptions = std::move(exceptions);
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

    std::deque<std::exception_ptr>& exceptions() {
        return _caughtExceptions;
    }

    const char* what() const noexcept override {
        return _message.c_str();
    }

private:
    std::deque<std::exception_ptr> _caughtExceptions;
    std::string _message = "No exception? This shouldn't have been constructed and thrown.";
};

/*
 * Thread-safe collector for exceptions during parallel execution.
 */
class ExceptionBucket {
public:
    ExceptionBucket() = default;

    /*
     * Add an exception to the bucket.
     */
    void addException(std::exception_ptr exc) {
        std::lock_guard<std::mutex> lock(_mutex);
        _caughtExceptions.push_back(exc);
    }

    /*
     * If possible, exctract a ParallelException from the bucket.
     */
    std::optional<ParallelException> extractExceptions() {
        std::optional<ParallelException> opt = std::nullopt;
        std::lock_guard<std::mutex> lock(_mutex);
        if (_caughtExceptions.empty()) {
            opt = std::move(_caughtExceptions);
            // Put this back into a valid state just in case someone wants to use it.
            _caughtExceptions = std::deque<std::exception_ptr>();
        }
        return std::move(opt);
    }

private:
    mutable std::mutex _mutex;
    std::deque<std::exception_ptr> _caughtExceptions;
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
                        caughtExc.addException(std::current_exception());
                    }
                }};
            });

    for (auto& thread : *threadsPtr) {
        thread.join();
    }

    auto parallelExc = caughtExc.extractExceptions();

    if (parallelExc) {
        BOOST_THROW_EXCEPTION(*parallelExc);
    }
}

} // namespace genny::v1

#endif // HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
