#ifndef HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
#define HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED

#include <cstdint>
#include <memory>
#include <random>

namespace genny {
namespace V1 {

/**
 * Genny random number generator.
 *
 * @tparam RNGImpl Random number generator implementation to use.
 */
template <class RNGImpl>
class Random {

public:
    using result_type = typename RNGImpl::result_type;

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
}  // namespace V1

/**
 * DefaultRandom should be used if you need a random number generator.
 */
using DefaultRandom = V1::Random<std::mt19937_64>;

}  // namespace genny
#endif  // HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
