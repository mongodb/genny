#ifndef HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED
#define HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED

#include <yaml-cpp/yaml.h>

#include <gennylib/metrics.hpp>
#include "Orchestrator.hpp"

namespace genny {

class WorkloadConfig {

public:
    explicit WorkloadConfig(const YAML::Node& node,
                         metrics::Registry& registry,
                         Orchestrator& orchestrator);

    // no rvals
    WorkloadConfig(YAML::Node&& node,
                    metrics::Registry&& registry,
                    Orchestrator&& orchestrator) = delete;

    Orchestrator* orchestrator();
    metrics::Registry* registry();

    const YAML::Node operator[](const std::string& key) {
        return this->_node->operator[](key);
    }

private:
    const YAML::Node* const _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
};

}  // genny


#endif  // HEADER_04C57DA8_96C0_4AD2_BC67_577F320BAFF8_INCLUDED
