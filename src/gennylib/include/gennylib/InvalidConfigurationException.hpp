#ifndef HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
#define HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED

#include <exception>
#include <stdexcept>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidConfigurationException : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

}  // namespace genny


#endif  // HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
