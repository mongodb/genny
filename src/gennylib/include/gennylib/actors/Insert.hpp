#ifndef HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
#define HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED

#include <iostream>
#include <memory>
#include <random>

#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

class Insert : public genny::Actor {

public:
    explicit Insert(ActorContext& context, unsigned int thread);

    ~Insert() = default;

    virtual void run() override;

private:
    std::mt19937_64 _rng;
    metrics::Timer _insertTimer;
    metrics::Counter _operations;
    mongocxx::pool::entry _client;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
