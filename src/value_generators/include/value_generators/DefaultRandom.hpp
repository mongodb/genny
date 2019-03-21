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

#ifndef HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
#define HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED

#include <cstdint>
#include <memory>

#include <boost/random.hpp>

namespace genny {
namespace v1 {

/**
 * Genny random number generator.
 *
 * @tparam RNGImpl Random number generator implementation to use.
 * @private
 */
template <class RNGImpl>
class Random {

public:
    using result_type = typename RNGImpl::result_type;

    /** @private */
    using Handle = Random&;

    /**
     * Construct a Random object.
     * @param seed the seed. The default seed is used if seed is omitted.
     */
    explicit Random(result_type seed = 6514393) : _rng(seed) {}

    // No moves or copies.
    Random(Random&&) = delete;
    Random& operator=(Random&&) = delete;

    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;

    ~Random() = default;

    /**
     * Construct new Random using the next number from the current one as the seed.
     * @return
     */
    Random child() {
        return Random(this->nextValue());
    }

    /**
     * Seed engine.
     */
    void seed(result_type newSeed) {
        _rng.seed(newSeed);
    }

    /**
     * Generate random number.
     */
    result_type nextValue() {
        return _rng();
    }

    /**
     * Generate random number, shorthand for Random::nextValue()
     */
    result_type operator()() {
        return this->nextValue();
    }

    /**
     * Minimum value.
     */
    static constexpr auto min() {
        return RNGImpl::min();
    }

    /**
     * Maximum value.
     */
    static constexpr auto max() {
        return RNGImpl::max();
    }

private:
    // RNGImpl is a plain class member instead of a unique pointer to avoid performance penalty.
    // For more detail, see https://github.com/10gen/genny/pull/88#issuecomment-451014165
    RNGImpl _rng;
};
}  // namespace v1

/**
 * DefaultRandom should be used if you need a random number generator.
 */
// Note he use boost::random because its distributions are
// cross-platform. The downside to this is that some <algorithm>
// functions e.g. std::shuffle() don't work with DefaultRandom
// because Boost's RNGs don't have constexpr min() and max()
// functions - https://svn.boost.org/trac10/ticket/12251
using DefaultRandom = v1::Random<boost::random::mt19937_64>;

}  // namespace genny
#endif  // HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
