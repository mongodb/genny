#include <memory>

#include <gennylib/actors/BigQuery.hpp>

namespace {

}  // namespace

struct genny::actor::BigQuery::PhaseConfig {
    PhaseConfig(PhaseContext& context, int thread)
    {}
};

void genny::actor::BigQuery::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // TODO: main logic
        }
    }
}

genny::actor::BigQuery::BigQuery(genny::ActorContext& context, const unsigned int thread)
    : _loop{context, thread} {}

genny::ActorVector genny::actor::BigQuery::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "BigQuery") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::BigQuery>(context, i));
    }
    return out;
}
