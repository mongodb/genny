#ifndef HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
#define HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED

#include <cstdint>
#include <random>

namespace genny {

template <class RNGImpl>
class RNG {

public:
    using result_type = typename RNGImpl::result_type;

    explicit RNG(result_type seed = 6514393) : _rng(std::make_unique<RNGImpl>(seed)) {}

    RNG child() {
        return RNG(*this());
    }

    void seed(result_type newSeed) {
        _rng->seed(newSeed);
    }

    auto operator()() -> result_type {
        return (*_rng)();
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

using DefaultRNG = RNG<std::mt19937_64>;

}  // namespace genny
#endif  // HEADER_EBA231D0_AA7A_4008_A9E8_BD1C98D9023E_INCLUDED
