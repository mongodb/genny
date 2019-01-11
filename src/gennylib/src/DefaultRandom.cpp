#include "gennylib/DefaultRandom.hpp"

namespace genny::V1 {

// Use explicit specialization to speed up compilation.
template <>
class Random<std::mt19937_64>;

}  // namespace genny::V1