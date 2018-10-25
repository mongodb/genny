#pragma once

#include <map>
#include <memory>

#include <gennylib/ActorProducer.hpp>

namespace genny{

class Cast {
public:
    using Producer = ActorProducer;
    using ProducerMap = ActorProducerMap;

    struct Registration;

    void add(const std::string & castName, std::shared_ptr<Producer> entry) {
        auto res = _producers.emplace(castName, std::move(entry));

        if (!res.second) {
            std::ostringstream stream;
            auto previousProducer = res.first->second;
            stream << "Failed to add '" << entry->name() << "' as '" << castName << "', '"
                   << previousProducer->name() << "' already added instead.";
            throw std::domain_error(stream.str());
        }
    }

    template <typename ActorT>
    static Registration makeDefaultRegistration(const std::string& name);

    std::shared_ptr<Producer> getProducer(const std::string& name) const {
        try {
            return _producers.at(name);
        } catch (const std::out_of_range) {
            return {};
        }
    }

    const ProducerMap& getProducers() const {
        return _producers;
    }

private:
    ProducerMap _producers;
};

inline Cast & getCast(){
    static Cast _cast;
    return _cast;
}

struct Cast::Registration {
    Registration(const std::string& name, std::shared_ptr<Producer> producer) {
        getCast().add(name, std::move(producer));
    }
};

template <typename ActorT>
Cast::Registration Cast::makeDefaultRegistration(const std::string& name) {
    return Registration(name, std::make_shared<DefaultThreadedActorProducer<ActorT>>(name));
}

} // genny
