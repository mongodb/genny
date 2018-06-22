#include <gennylib/ActorConfig.hpp>
#include <gennylib/Orchestrator.hpp>

namespace genny {


const YAML::Node& ActorConfig::operator->() const {
    return *this->_node;
}

ActorConfig::ActorConfig(const YAML::Node& node,
                         metrics::Registry& registry,
                         Orchestrator& orchestrator)
: _node{std::addressof(node)},
  _registry{std::addressof(registry)},
  _orchestrator{std::addressof(orchestrator)} {}

metrics::Registry* ActorConfig::registry() {
    return _registry;
}

Orchestrator *ActorConfig::orchestrator() {
    return _orchestrator;
}

}  // genny


