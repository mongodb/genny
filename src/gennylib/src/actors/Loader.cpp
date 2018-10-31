#include <memory>

#include <gennylib/actors/Loader.hpp>

namespace {

}  // namespace

struct genny::actor::Loader::PhaseConfig {
    PhaseConfig(PhaseContext& context, int thread)
    {}
};

void genny::actor::Loader::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // TODO: main logic
        }
    }
}

genny::actor::Loader::Loader(genny::ActorContext& context, const unsigned int thread)
    : _loop{context, thread} {}

genny::ActorVector genny::actor::Loader::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Loader") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::Loader>(context, i));
    }
    return out;
}
