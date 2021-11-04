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
     * Given iterable, run op in one thread per element of iterable, passing
     * a reference to the element for each execution.
     *
     * Any exception thrown in any thread is gathered and rethrown in the calling thread.
     */
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
        this->_container = rhs._container;
    }
    AtomicContainer(AtomicContainer&& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
        this->_container = std::move(rhs._container);
    }

    AtomicContainer<T>& operator=(const AtomicContainer& rhs) {
        std::lock_guard<lock_t> lock(rhs._mutex);
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
    auto&& at(size_t pos) {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.at(pos);
    }
    void pop_front() {
        std::lock_guard<lock_t> lock(_mutex);
        _container.pop_front();
        return;
    }
    size_t size() const {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.size();
    }
    bool empty() const {
        std::lock_guard<lock_t> lock(_mutex);
        return _container.empty();
    }

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
        if (_forciblyHeld) {
            BOOST_THROW_EXCEPTION(
                std::logic_error("Cannot forcibly hold AtomicContainer lock multiple times."));
        }
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
    using lock_t = std::recursive_mutex;
    mutable bool _forciblyHeld = false;
    T _container;
    mutable lock_t _mutex;
};

template <typename T>
using AtomicDeque = AtomicContainer<std::deque<T>>;

template <typename T>
using AtomicVector = AtomicContainer<std::vector<T>>;


} // namespace genny::v1

#endif // HEADER_5129031F_B241_46DD_8285_64596CB0C155_INCLUDED
