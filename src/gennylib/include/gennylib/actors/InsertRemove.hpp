#ifndef HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
#define HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED

#include <iostream>

#include <mongocxx/pool.hpp>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class InsertRemove : public genny::PhasedActor {

public:
    explicit InsertRemove(ActorContext& context, const unsigned int thread);

    ~InsertRemove() = default;

    static ActorVector producer(ActorContext& context);

private:
    struct Config;

    void doPhase(PhaseNumber phase);
    std::mt19937_64 _rng;

    metrics::Timer _insertTimer;
    metrics::Timer _removeTimer;
    mongocxx::pool::entry _client;
    std::unique_ptr<Config> _config;
};

}  // namespace genny::actor

#endif  // HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
