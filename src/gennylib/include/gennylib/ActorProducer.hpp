#ifndef HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
#define HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED

#include <memory>
#include <string_view>

#include <gennylib/ActorVector.hpp>

namespace genny {

class ActorContext;

/**
 * `ActorProducer.produce()` maps from ActorContext -> vector of `Actor`s.
 *
 * For the following %YAML,
 *
 * ```yaml
 * SchemaVersion: 2018-07-01
 * Actors:
 * - Name: Foo
 * - Name: Bar
 * ```
 *
 * each ActorProducer will be called twice: once with the ActorContext for
 * `{Name: Foo}` and another with the ActorContext for `{Name: Bar}`.
 *
 * Many `ActorProducer`s will want to return an empty ActorVector if the
 * "Name" field is different from what they expect, but this is just
 * a convention.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call `ActorContext::get()` only during their constructors and retain
 * refs or copies of config objects
 */
class ActorProducer {
public:
    ActorProducer(std::string_view name) : _name{name} {}
    ActorProducer(const ActorProducer&) = delete;
    ActorProducer& operator=(const ActorProducer&) = delete;
    ActorProducer(ActorProducer&&) = default;
    ActorProducer& operator=(ActorProducer&&) = default;

    const std::string_view& name() const {
        return _name;
    }

    virtual ActorVector produce(ActorContext&) = 0;

private:
    std::string_view _name;
};

/** @private */
class ParallelizedActorProducer : public ActorProducer {
public:
    using ActorProducer::ActorProducer;

    virtual void produceInto(ActorVector& out, ActorContext& context) = 0;
    ActorVector produce(ActorContext& context) override;
};

/** @private */
template <class ActorT>
class DefaultActorProducer : public ParallelizedActorProducer {
public:
    using ParallelizedActorProducer::ParallelizedActorProducer;
    DefaultActorProducer() : DefaultActorProducer{ActorT::defaultName()} {}

    void produceInto(ActorVector& out, ActorContext& context) override {
        out.emplace_back(std::make_unique<ActorT>(context));
    }
};

}  // namespace genny

#endif  // HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
