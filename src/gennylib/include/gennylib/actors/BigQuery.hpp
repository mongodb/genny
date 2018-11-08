#ifndef HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610
#define HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * TODO: document me
 */
class BigQuery : public Actor {

public:
    explicit BigQuery(ActorContext& context, const unsigned int thread);
    ~BigQuery() = default;

    void run() override;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610
