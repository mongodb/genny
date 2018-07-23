#ifndef HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
#define HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED

#include <iostream>

#include <mongocxx/client_session.hpp>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class Insert : public genny::PhasedActor {

public:
    explicit Insert(ActorContext& context, const std::string& name = "insert");

    ~Insert() = default;

    static ActorVector producer(ActorContext& context);

private:
    void doPhase(int phase);

    metrics::Timer _outputTimer;
    metrics::Counter _operations;
    mongocxx::client_session* session;
    std::vector<std::string> documents;
};

}  // namespace genny::actor

#endif  // HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
