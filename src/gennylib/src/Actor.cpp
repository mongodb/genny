#include <gennylib/Actor.hpp>

#include <atomic>

#include <gennylib/context.hpp>

namespace genny{

Actor::Actor(ActorContext& context) : _id{context.workload().nextActorId()} {}

} // namespace genny
