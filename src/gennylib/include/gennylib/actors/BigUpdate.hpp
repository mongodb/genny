#ifndef HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46
#define HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * TODO: document me
 */
class BigUpdate : public Actor {

public:
    explicit BigUpdate(ActorContext& context, const unsigned int thread);
    ~BigUpdate() = default;

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
