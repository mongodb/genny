#ifndef HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
#define HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED

#include <iostream>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

class InsertRemove : public Actor {

public:
    explicit InsertRemove(ActorContext& context, const unsigned int thread);

    ~InsertRemove() = default;

    static ActorVector producer(ActorContext& context);
    void run() override;
    
private:
    struct PhaseConfig;
    std::mt19937_64 _rng;

    metrics::Timer _insertTimer;
    metrics::Timer _removeTimer;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
