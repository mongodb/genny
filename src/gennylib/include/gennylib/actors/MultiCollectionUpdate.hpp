#ifndef HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46
#define HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * MultiCollectionUpdate is an actor the performs updates across parameterizable number of
 * collections. Updates are performed in a loop using {@code PhaseLoop} and each iteration picks a
 * random collection to update. The actor records the ltency of each update, and the total number of
 * documents updated.
 */
class MultiCollectionUpdate : public Actor {

public:
    explicit MultiCollectionUpdate(ActorContext& context, const unsigned int thread);
    ~MultiCollectionUpdate() = default;

    void run() override;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    std::mt19937_64 _rng;

    metrics::Timer _updateTimer;
    metrics::Counter _updateCount;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46
