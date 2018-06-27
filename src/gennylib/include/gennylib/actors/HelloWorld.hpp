#ifndef HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
#define HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED

#include <iostream>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class HelloWorld : public genny::PhasedActor {

public:
    HelloWorld(const ActorConfig& config, const std::string& name = "hello");

    ~HelloWorld() override = default;

private:
    void doPhase(int phase) override;

    metrics::Timer _output_timer;
    metrics::Counter _operations;
};

}  // namespace genny::actor

#endif  // HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
