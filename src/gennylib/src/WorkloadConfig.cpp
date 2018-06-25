#include <gennylib/WorkloadConfig.hpp>
#include <gennylib/Orchestrator.hpp>

namespace genny {

WorkloadConfig::WorkloadConfig(const YAML::Node& node,
                         metrics::Registry& registry,
                         Orchestrator& orchestrator)
: _node{std::addressof(node)},
  _registry{std::addressof(registry)},
  _orchestrator{std::addressof(orchestrator)} {}

metrics::Registry* WorkloadConfig::registry() {
    return _registry;
}

Orchestrator *WorkloadConfig::orchestrator() {
    return _orchestrator;
}


}  // genny


