#ifndef HEADER_3C02F389_824D_4F81_B9C5_DBD72AABD154_INCLUDED
#define HEADER_3C02F389_824D_4F81_B9C5_DBD72AABD154_INCLUDED

#include <string_view>

#include <yaml-cpp/yaml.h>

#include <gennylib/ActorProducer.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

template<class ProducerT>
class ContextHelper {

static_assert(std::is_constructible_v<ProducerT,const std::string_view&>);

public:
    explicit ContextHelper(const std::string_view& name, const std::string_view& yaml)
        : _producer{std::make_shared<ProducerT>(name)},
          _registration{globalCast().makeRegistration(_producer)},
          _node{YAML::Load(std::string{yaml})},
          _registry{},
          _orchestratorGauge{_registry.gauge("Genny.Orchestrator")},
          _orchestrator{_orchestratorGauge},
          _workloadContext{
              _node, _registry, _orchestrator, "mongodb://localhost:27017", genny::globalCast()}
    {}

    WorkloadContext& workloadContext() {
        return _workloadContext;
    }

    void run() {
        for(auto&& actor : _workloadContext.actors()) {
            actor->run();
        }
    }

    // add other helper methods here (e.g. exposing Orchestrator etc) as-needed.

private:
    std::shared_ptr<ProducerT> _producer;
    Cast::Registration _registration;
    YAML::Node _node;
    metrics::Registry _registry;
    metrics::Gauge _orchestratorGauge;
    Orchestrator _orchestrator;
    WorkloadContext _workloadContext;
};

}  // namespace genny

#endif  // HEADER_3C02F389_824D_4F81_B9C5_DBD72AABD154_INCLUDED
