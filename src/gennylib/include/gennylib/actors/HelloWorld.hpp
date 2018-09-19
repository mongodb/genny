#ifndef HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
#define HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED

#include <iostream>

#include <mongocxx/client_session.hpp>

#include <gennylib/PhaseLoop.hpp>

namespace genny::actor {

class HelloWorld : public genny::Actor {

public:
    explicit HelloWorld(ActorContext& context, unsigned int thread);

    virtual void run() override;

    ~HelloWorld() = default;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    genny::PhaseLoop<PhaseConfig> _loop;
    mongocxx::client_session* session;
};

}  // namespace genny::actor

#endif  // HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
