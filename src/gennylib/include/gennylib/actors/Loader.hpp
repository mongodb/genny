#ifndef HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
#define HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * TODO: document me
 */
class Loader : public Actor {

public:
    explicit Loader(ActorContext& context, const unsigned int thread);
    ~Loader() = default;

    void run() override;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
