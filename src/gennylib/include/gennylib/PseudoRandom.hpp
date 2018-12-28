#ifndef HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
#define HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED

#include <cstdint>
#include <memory>
#include <random>

namespace genny {

template <class RNGImpl>
class PseudoRandom {

public:
    using result_type = typename RNGImpl::result_type;

    using Handle = PseudoRandom&;

    explicit PseudoRandom(result_type seed = 6514393) : _rng(std::make_unique<RNGImpl>(seed)) {}

    // Allow moves.
    PseudoRandom(PseudoRandom&&) = default;
    PseudoRandom& operator=(PseudoRandom&&) = default;

    // No copies.
    PseudoRandom(const PseudoRandom&) = delete;
    PseudoRandom& operator=(const PseudoRandom&) = delete;

    ~PseudoRandom() = default;

    PseudoRandom child() {
        return PseudoRandom(this());
    }

    void seed(result_type newSeed) {
        _rng->seed(newSeed);
    }

    result_type nextValue() {
        return (*_rng)();
    }

    result_type operator()() {
        return this->nextValue();
    }

    static constexpr auto min() {
        return RNGImpl::min();
    }

    static constexpr auto max() {
        return RNGImpl::max();
    }

private:
    std::unique_ptr<RNGImpl> _rng;
};

using DefaultRandom = PseudoRandom<std::mt19937_64>;

}  // namespace genny
#endif  // HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
