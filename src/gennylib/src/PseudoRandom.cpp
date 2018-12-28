#include "gennylib/PseudoRandom.hpp"

namespace genny {

// Use explicit specialization to speed up compilation.
template<> class PseudoRandom<std::mt19937_64>;

}