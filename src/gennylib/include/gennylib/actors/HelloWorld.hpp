#ifndef HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
#define HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED

#include <iostream>

#include <mongocxx/client_session.hpp>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class HelloWorld : public genny::PhasedActor {

public:
    HelloWorld(ActorContext& context, const std::string& name = "hello");

    ~HelloWorld() override = default;

    static ActorVector producer(ActorContext& context);

private:
    void doPhase(int phase) override;

    metrics::Timer _outputTimer;
    metrics::Counter _operations;
    // TODO: for now this is dummy / smoke-test that we have the driver
    mongocxx::client_session* session;
    std::string _message;
};

}  // namespace genny::actor

#endif  // HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
