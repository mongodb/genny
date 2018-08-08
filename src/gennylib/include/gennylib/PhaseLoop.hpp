#ifndef HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
#define HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED

#include <cassert>
#include <chrono>
#include <iterator>
#include <optional>
#include <sstream>

#include <gennylib/InvalidConfigurationException.hpp>

namespace genny {

namespace V1 {

/**
 * Tracks the iteration-state of a `PhaseLoop`.
 */
// This is intentionally header-only to help avoid doing unnecessary function-calls.
class LoopIterator {

public:
    // <iterator-concept>
    struct Value {};  // intentionally empty; most compilers will elide any actual storage
    typedef std::forward_iterator_tag iterator_category;
    typedef Value value_type;
    typedef Value reference;
    typedef Value pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

    explicit LoopIterator(bool isEnd,
                          std::optional<int> maxIters,
                          std::optional<std::chrono::milliseconds> maxDuration)
        : _isEndIterator{isEnd},
          _minDuration{std::move(maxDuration)},
          _minIterations{std::move(maxIters)},
          _currentIteration{0},
          _startedAt{_minDuration ? std::chrono::steady_clock::now()
                                  : std::chrono::time_point<std::chrono::steady_clock>::min()} {
        // invariant checked in PhaseLoop
        assert(isEnd || _minDuration || _minIterations);
    }

    explicit LoopIterator(bool isEnd) : LoopIterator{isEnd, std::nullopt, std::nullopt} {}

    Value operator*() const {
        return Value();
    }

    LoopIterator& operator++() {
        if (_minIterations) {
            ++_currentIteration;
        }
        return *this;
    }

    // clang-format off
    bool operator==(const LoopIterator& rhs) const {
        // I heard you like terse, short-circuiting business-logic, bro, so I wrote you a love-letter to || ❤️
        return
            // Comparing this == .end(). This is most common call in range-based for-loops, so do it first.
            (rhs._isEndIterator
             // Need to see if this is in end state. There are two conditions
             // 1. minIterations is empty-optional or we're at or past minIterations
             && (!_minIterations ||
                _currentIteration >= *_minIterations)
             // 2. minDuration is empty-optional or we've exceeded minDuration
             && (!_minDuration ||
              // check is last in chain to avoid doing now() call unnecessarily
              std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - _startedAt) >= *_minDuration))

            // Below checks are mostly for pure correctness;
            //   "well-formed" code will only use this iterator in range-based for-loops and will thus
            //   never use these conditions.

            // this == this
            || (this == &rhs)

            // neither is end iterator but have same fields
            || (!rhs._isEndIterator && !_isEndIterator
                    && _minDuration      == rhs._minDuration
                    && _startedAt        == rhs._startedAt
                    && _minIterations    == rhs._minIterations
                    && _currentIteration == rhs._currentIteration)

            // both .end() iterators (all .end() iterators are ==)
            || (_isEndIterator && rhs._isEndIterator)

            // we're .end(), so 'recurse' but flip the args so 'this' is rhs
            || (_isEndIterator && rhs == *this);
    }
    // clang-format on

    // Iterator concepts only require !=, but the logic is much easier to reason about
    // for ==, so just negate that logic 😎 (compiler should inline it)
    bool operator!=(const LoopIterator& rhs) const {
        return !(*this == rhs);
    }

private:
    const bool _isEndIterator;

    const std::optional<std::chrono::milliseconds> _minDuration;
    std::chrono::steady_clock::time_point _startedAt;

    const std::optional<int> _minIterations;
    int _currentIteration;
};

}  // namespace V1


/**
 * Configured with an optional<min#iterations> and/or optional<min duration>. The
 * returned .begin() iterators will not == .end() until both the # iterations and
 * duration requirements are met.
 *
 * Can be used as-is but intended to be used from `context.hpp` classes and
 * configured from conventions.
 *
 * See extended example in `PhaseContext.loop()`.
 */
class PhaseLoop {

public:
    // Ctor is ideally only called during Actor constructors so fine to take our time here.
    explicit PhaseLoop(std::optional<int> minIterations,
                       std::optional<std::chrono::milliseconds> minDuration)
        : _minIterations{std::move(minIterations)}, _minDuration{std::move(minDuration)} {

        // both optionals empty (no termination condition; we'd iterate forever
        //   (or not at all depending on how you interpret it)
        if (!_minIterations && !_minDuration) {
            // May want to support this in the future once there's better support for Actors to run
            // in the "background" forever / for the duration of a phase. For now it's likely a
            // configuration error.
            throw InvalidConfigurationException(
                "Need to specify either min iterations or min duration");
        }
        if (_minIterations && *_minIterations < 0) {
            std::stringstream str;
            str << "Need non-negative number of iterations. Gave " << *_minIterations;
            throw InvalidConfigurationException(str.str());
        }
        if (_minDuration && _minDuration->count() < 0) {
            std::stringstream str;
            str << "Need non-negative duration. Gave " << _minDuration->count() << "milliseconds";
            throw InvalidConfigurationException(str.str());
        }
    }

    V1::LoopIterator begin() {
        return V1::LoopIterator{false, _minIterations, _minDuration};
    }

    V1::LoopIterator end() {
        return V1::LoopIterator{true};
    }

private:
    std::optional<int> _minIterations;
    std::optional<std::chrono::milliseconds> _minDuration;
};

}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
