#ifndef HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
#define HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED

#include <iostream>
#include <memory>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/PseudoRandom.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

class Insert : public genny::Actor {

public:
    explicit Insert(ActorContext& context);
    ~Insert() = default;

    static std::string_view defaultName() {
        return "Insert";
    }
    void run() override;

private:
    genny::DefaultRandom _rng;

    ExecutionStrategy _strategy;
    mongocxx::pool::entry _client;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
