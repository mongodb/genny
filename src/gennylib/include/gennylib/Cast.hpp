#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <gennylib/ActorProducer.hpp>

namespace genny{

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
