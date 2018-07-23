#ifndef HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED
#define HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED

#include <memory>
#include <vector>

#include <gennylib/Actor.hpp>

namespace genny {

/**
 * A convenience typedef for the return value from ActorProducer.
 */
using ActorVector = typename std::vector<std::unique_ptr<Actor>>;


}  // namespace genny

#endif  // HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED
