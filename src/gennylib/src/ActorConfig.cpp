#include <gennylib/ActorConfig.hpp>

namespace genny {

ActorConfig::ActorConfig(const YAML::Node& node)
: _node{std::addressof(node)} {}

const YAML::Node& ActorConfig::operator->() const {
    return *this->_node;
}

}  // genny


