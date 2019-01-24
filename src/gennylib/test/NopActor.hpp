#ifndef HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED
#define HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

class NopActor : public Actor {
public:
    explicit NopActor(ActorContext& c) : Actor(c), _loop{c} {}

    void run() override {
        for (auto&& p : _loop) {
            for (auto&& _ : p) {
                // nop
            }
        }
    }

    static std::string_view defaultName() {
        return "NopActor";
    }

    static std::shared_ptr<ActorProducer> producer() {
        static std::shared_ptr<ActorProducer> _producer =
            std::make_shared<genny::DefaultActorProducer<genny::actor::NopActor>>();
        return _producer;
    }

private:
    struct PhaseConfig {
        explicit PhaseConfig(PhaseContext&) {}
    };
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED