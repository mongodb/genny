#ifndef HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED
#define HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED

#include <string_view>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/DefaultRandom.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

#include <cast_core/config/RunCommandConfig.hpp>

namespace genny::actor {

/**
 * TODO: document me
 */
class CrudActor : public Actor {
public:
    class Operation {

    public:
        using OpCallback = std::function<void(CrudActor::Operation&)>;
        using Options = config::RunCommandConfig::Operation;
        struct Fixture {
            PhaseContext& phaseContext;
            mongocxx::database database;
            OpCallback& op;
        };

        OpCallback& callback;
        const YAML::Node& opCommand;

    public:
        Operation(Fixture fixture, const YAML::Node& opCommand, Options opts);
        void run(const mongocxx::client_session& session);

    private:
        mongocxx::database _database;
        std::unique_ptr<value_generators::DocumentGenerator> _doc;
        Options _options;
        std::optional<metrics::Timer> _timer;
    };

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
