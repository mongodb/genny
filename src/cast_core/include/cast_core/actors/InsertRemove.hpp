#ifndef HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
#define HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED

#include <iostream>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/DefaultRandom.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * InsertRemove is a simple actor that inserts and then removes the same document from a
 * collection. It uses `PhaseLoop` for looping.  Each instance of the actor uses a
 * different document, indexed by an integer `_id` field. The actor records the latency of each
 * insert and each remove.
 */
class InsertRemove : public Actor {

public:
    explicit InsertRemove(ActorContext& context);
    ~InsertRemove() = default;

    static std::string_view defaultName() {
        return "InsertRemove";
    }
    void run() override;

private:
    genny::DefaultRandom _rng;

    ExecutionStrategy _insertStrategy;
    ExecutionStrategy _removeStrategy;
    mongocxx::pool::entry _client;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
