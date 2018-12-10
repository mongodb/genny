#ifndef HEADER_32412A69_F128_4BC8_8335_520EE35F5381
#define HEADER_32412A69_F128_4BC8_8335_520EE35F5381

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * Command is an actor that performs database and admin commands on a database.
 * TODO: finish header description.
 */

class RunCommand : public Actor {

public:
    explicit RunCommand(ActorContext& context, const unsigned int thread);
    ~RunCommand() = default;

    void run() override;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    std::mt19937_64 _rng;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};


}  // namespace genny::actor

#endif  // HEADER_32412A69_F128_4BC8_8335_520EE35F5381