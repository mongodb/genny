#ifndef HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED
#define HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED

#include <functional>
#include <string>
#include <unordered_map>

#include <gennylib/WorkloadConfig.hpp>
#include <gennylib/PhasedActor.hpp>
#include <iostream>

namespace genny {
class WorkloadConfig;

template<typename T>
class ActorFactory {

public:
    using Actor = std::unique_ptr<T>;
    using ActorList = std::vector<Actor>;
    using Producer = std::function<Actor(WorkloadConfig*)>;

    void addProducer(std::string name, const Producer &function) {
        _producers.emplace(std::move(name), function);
    }

    ActorList actors(WorkloadConfig* config) {
        auto out = ActorList {};
        const auto actors = (*config)["Actors"];
        for(const auto& block : actors) {
            const auto producer =  _producers[block["Name"].as<std::string>()];
            int count = block["Count"] ? block["Count"].as<int>() : 1;
            for(int i=0; i<count; ++i) {
                out.push_back(std::move(producer(config)));
            }
        }
        return out;
    }

private:
    std::unordered_map<std::string, Producer> _producers;
};

}  // genny

#endif  // HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED
