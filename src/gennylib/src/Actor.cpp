#include <gennylib/Actor.hpp>

#include <atomic>

namespace genny{

ActorId nextActorId() {
    static std::atomic<ActorId> nextActorId{};
    return nextActorId++;
}

Actor::Actor() : _id{nextActorId()}{}

} // namespace genny
