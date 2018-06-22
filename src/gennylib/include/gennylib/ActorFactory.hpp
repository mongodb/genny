#ifndef HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED
#define HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED

#include <functional>
#include <map>

#include <gennylib/ActorConfig.hpp>
#include <gennylib/PhasedActor.hpp>
#include <iostream>

namespace genny {

template<typename T>
class ActorFactory {

public:
    using ActorList = std::vector<std::unique_ptr<T>>;
    using Producer = std::function<ActorList(class ActorConfig*)>;

    void hook(const Producer& function) {
        _producers.push_back(function);
    }

    ActorList actors(ActorConfig* config) {
        auto out = ActorList {};
        for(const Producer& producer : _producers) {
            for(auto&& p : producer(config)) {
                out.push_back(std::move(p));
            }
        }
        return out;
    }

private:
    std::vector<const Producer> _producers;
};

}  // genny

#endif  // HEADER_96652FB2_838C_460F_9CDD_5BD155394E7C_INCLUDED
