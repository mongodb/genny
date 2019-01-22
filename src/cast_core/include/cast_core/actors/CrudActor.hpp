#ifndef HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED
#define HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED

#include <string_view>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <value_generators/DefaultRandom.hpp>
#include <value_generators/value_generators.hpp>

#include <cast_core/config/RunCommandConfig.hpp>

namespace genny::actor {

/**
 * TODO: document me
 */
class CrudActor : public Actor {

public:
    class Operation;

public:
    explicit CrudActor(ActorContext& context);
    ~CrudActor() = default;

    static std::string_view defaultName() {
        return "CrudActor";
    }

    void run() override;

private:
    ExecutionStrategy _strategy;
    mongocxx::pool::entry _client;
    genny::DefaultRandom _rng;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED
