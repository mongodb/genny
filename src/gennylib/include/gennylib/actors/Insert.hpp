#ifndef HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
#define HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED

#include <iostream>
#include <memory>
#include <random>

#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class Insert : public genny::PhasedActor {

public:
    explicit Insert(ActorContext& context, const unsigned int thread);

    ~Insert() = default;

    static ActorVector producer(ActorContext& context);

private:
    struct Config;

    void doPhase(int phase);
    std::mt19937_64 _rng;
    metrics::Timer _outputTimer;
    metrics::Counter _operations;
    mongocxx::pool::entry _client;
    std::unique_ptr<Config> _config;
};

}  // namespace genny::actor

#endif  // HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
