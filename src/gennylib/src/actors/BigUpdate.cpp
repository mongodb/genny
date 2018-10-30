#include <memory>

#include <gennylib/actors/BigUpdate.hpp>

namespace {

}  // namespace

struct genny::actor::BigUpdate::PhaseConfig {
    PhaseConfig(PhaseContext& context, int thread)
    {}
};

void genny::actor::BigUpdate::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // TODO: main logic
        }
    }
}

genny::actor::BigUpdate::BigUpdate(genny::ActorContext& context, const unsigned int thread)
    : _loop{context, thread} {}

genny::ActorVector genny::actor::BigUpdate::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "BigUpdate") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::BigUpdate>(context, i));
    }
    return out;
}
