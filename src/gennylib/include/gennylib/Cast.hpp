#pragma once

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include <gennylib/ActorProducer.hpp>

namespace genny {

/**
 * A cast is a map of strings to shared ActorProducer instances.
 *
 * This class is how one conveys to a driver/workload context which ActorProducers are available.
 * There will always be a global singleton cast available via getCast(). For limited applications
 * and testing, one can make local Cast instances that behave in an identical fashion.
 *
 * To easily register a default ActorProducer to the global Cast, one can use the following in
 * a source file:
 * ```
 * auto registerMyActor = genny::Cast::registerDefault<MyActorT>();
 * ```
 * The function makes a specialization of DefaultActorProducer and hands it to the
 * Cast::Registration struct along with the `defaultName()` of MyActorT. When that struct
 * initializes, it adds its producer to the map in cast. More complex ActorProducers
 * can be registered by following this same pattern.
 *
 * ActorProducers are deliberately created and managed inside `shared_ptr`s. Generally speaking,
 * this means that an ActorProducer will live at least as long as each and every Cast that holds it.
 * If there is logic that happens at shutdown, ActorProducers may have copies of their `shared_ptr`
 * in other places and may live far longer than any Cast.
 *
 * Note that ActorProducers are allowed to be stateful. Invocations of the `produce()` member
 * function are not idempotent and may produce differently initialized Actors according to the
 * ActorProducer implementation.
 */
class Cast {
public:
    struct Registration;

    using ActorProducerMap = std::map<std::string_view, std::shared_ptr<ActorProducer>>;
    using List = std::initializer_list<Cast::ActorProducerMap::value_type>;

public:
    Cast() {}
    Cast(List init) {
        for (const auto& [name, producer] : init) {
            add(name, producer);
        }
    }

    void add(const std::string_view& castName, std::shared_ptr<ActorProducer> entry);

    std::shared_ptr<ActorProducer> getProducer(const std::string& name) const {
        return _producers.at(name);
    }

    const ActorProducerMap& getProducers() const {
        return _producers;
    }

    std::ostream& streamProducersTo(std::ostream&) const;

public:
    template <typename ActorT>
    static Registration registerDefault();

    /**
     * Register a custom ActorProducer. Do this if you don't wish to follow conventions
     * and wish to pass other state to your Actors other than just the ActorContext or if
     * you wish to create a custom number of instances instead of the number indicated by
     * the "Threads" Actor yaml.
     *
     * @tparam ProducerT type of the producer to use
     * @param producer shared ptr to call ProducerT.produce(c) for each ActorContext c.
     * @return Registration (empty struct used to call its ctor)
     */
    template <typename ProducerT>
    static Registration registerCustom(std::shared_ptr<ProducerT> producer);

private:
    ActorProducerMap _producers;
};

inline Cast& globalCast() {
    static Cast _cast;
    return _cast;
}

/**
 * `Cast::Registration` is a struct that registers a single ActorProducer to the global Cast.
 *
 * This struct is a vehicle for its ctor function which takes a name for the specific ActorProducer
 * in the Cast and a `shared_ptr` to the instance of the ActorProducer. This allows for pre-main
 * invocations of the registration via global variables of type `Cast::Registration`. The vast
 * majority of cases will want to use `registerDefault()` below and avoid most concerns on
 * this struct.
 */
struct Cast::Registration {
    Registration(const std::string_view& name, std::shared_ptr<ActorProducer> producer) {
        globalCast().add(name, std::move(producer));
    }
};

template <typename ProducerT>
Cast::Registration Cast::registerCustom(std::shared_ptr<ProducerT> producer) {
    return Registration(producer->name(), producer);
}

template <typename ActorT>
Cast::Registration Cast::registerDefault() {
    auto name = ActorT::defaultName();
    return Registration(name, std::make_shared<DefaultActorProducer<ActorT>>(name));
}

}  // namespace genny
