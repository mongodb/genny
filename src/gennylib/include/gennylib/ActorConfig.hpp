#ifndef HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED
#define HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED

#include <yaml-cpp/yaml.h>

#include <gennylib/metrics.hpp>
#include "Orchestrator.hpp"

namespace genny {

class ActorConfig {

public:
    explicit ActorConfig(const YAML::Node& node,
                         metrics::Registry& registry,
                         Orchestrator& orchestrator);

    const YAML::Node& operator->() const;

    Orchestrator* orchestrator();
    metrics::Registry* registry();

private:
    const YAML::Node* const _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
};

}  // genny


#endif  // HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED
