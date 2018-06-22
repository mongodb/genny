#include <yaml-cpp/yaml.h>

namespace genny {

class ActorConfig {

public:
    explicit ActorConfig(const YAML::Node& node);

    const YAML::Node& operator->() const;

private:
    const YAML::Node* _node;

};

}  // genny

