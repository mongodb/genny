#ifndef HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
#define HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED

#include <thread>

#include <yaml-cpp/yaml.h>

namespace genny {

class Cast;
void run_actor_helper(const YAML::Node& config, int token_count, const Cast& cast);
}  // namespace genny

#endif  // HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
