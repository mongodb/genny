#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <gennylib/ActorProducer.hpp>

namespace genny{

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
 * auto registerMyActor = genny::Cast::makeDefaultRegistration<MyActorT>();
 * ```
 * The function makes a specialization of DefaultActorProducer and hands it to the
 * Cast::Registration struct along with the `defaultName()` of MyActorT. When that struct
 * initializes, it adds its producer to the map in cast. More complex ActorProducers
 * can be registered by following this same pattern.
 *
 * Note that ActorProducers are stateful. Calls to produce upon them are not idempotent and may
 * produce differently initialized Actors as desired.
 */
class Cast {
public:
    struct Registration;

public:
    Cast(){}
    Cast(std::initializer_list<ActorProducerMap::value_type> init) {
        for (auto&& pair : init) {
            add(pair.first, std::move(pair.second));
        }
    }

    void add(const std::string_view & castName, std::shared_ptr<ActorProducer> entry) {
        auto res = _producers.emplace(castName, std::move(entry));

        if (!res.second) {
            std::ostringstream stream;
            auto previousActorProducer = res.first->second;
            stream << "Failed to add '" << entry->name() << "' as '" << castName << "', '"
                   << previousActorProducer->name() << "' already added instead.";
            throw std::domain_error(stream.str());
        }
    }

    std::shared_ptr<ActorProducer> getProducer(const std::string& name) const {
        try {
            return _producers.at(name);
        } catch (const std::out_of_range) {
            return {};
        }
    }

    const ActorProducerMap& getProducers() const {
        return _producers;
    }

public:
    template <typename ActorT>
    static Registration makeDefaultRegistrationAs(const std::string_view& name);
    template <typename ActorT>
    static Registration makeDefaultRegistration();

private:
    ActorProducerMap _producers;
};

inline Cast & getCast(){
    static Cast _cast;
    return _cast;
}

struct Cast::Registration {
    Registration(const std::string_view& name, std::shared_ptr<ActorProducer> producer) {
        getCast().add(name, std::move(producer));
    }
};

template <typename ActorT>
Cast::Registration Cast::makeDefaultRegistrationAs(const std::string_view& name) {
    return Registration(name, std::make_shared<DefaultActorProducer<ActorT>>(name));
}

template <typename ActorT>
Cast::Registration Cast::makeDefaultRegistration() {
    auto name = ActorT::defaultName();
    return makeDefaultRegistrationAs<ActorT>(name);
}

} // genny
