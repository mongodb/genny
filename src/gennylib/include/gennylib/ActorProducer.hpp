#ifndef HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
#define HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED

#include <memory>
#include <string>
#include <map>

#include <gennylib/ActorVector.hpp>

namespace genny {

class ActorContext;

/**
 * ActorProducer.produce() maps from ActorContext -> vector of Actors.
 *
 * For the following YAML,
 *
 * <pre>
 *      SchemaVersion: 2018-07-01
 *      Actors:
 *      - Name: Foo
 *      - Name: Bar
 * </pre>
 *
 * each ActorProducer will be called twice: once with the ActorContext for
 * {Name:Foo} and another with the ActorContext for {Name:Bar}.
 *
 * Many ActorProducers will want to return an empty ActorVector if the
 * "Name" field is different from what they expect, but this is just
 * a convention.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call context.get(...) only during their constructors and retain
 * refs or copies of config objects
 */
class ActorProducer {
public:
    ActorProducer(std::string name) : _name{name} {}
    ActorProducer(const ActorProducer&) = delete;
    ActorProducer& operator=(const ActorProducer&) = delete;
    ActorProducer(ActorProducer&&) = default;
    ActorProducer& operator=(ActorProducer&&) = default;

    const std::string& name() const {
        return _name;
    }

    virtual ActorVector produce(ActorContext &) = 0;

private:
    std::string _name;
};

template <class ActorT>
class DefaultActorProducer : public ActorProducer {
public:
    DefaultActorProducer(const std::string& name) : ActorProducer(name) {}

    ActorVector produce(ActorContext& context) override {
        ActorVector out;
        out.emplace_back(std::make_unique<ActorT>(context));
        return out;
    }
};

template <class ActorT>
class DefaultThreadedActorProducer : public ActorProducer {
public:
    DefaultThreadedActorProducer(const std::string& name) : ActorProducer(name) {}

    ActorVector produce(ActorContext& context) override {
        static int threadMock = 0;

        ActorVector out;
        out.emplace_back(std::make_unique<ActorT>(context, threadMock++));
        return out;
    }
};

using ActorProducerMap = std::map<std::string, std::shared_ptr<ActorProducer>>;

}  // namespace genny

#endif  // HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
