#ifndef HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
#define HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED

#include <exception>

namespace genny {

/**
 * Throw this to indicate bad configuration.
 */
class InvalidConfigurationException : public std::invalid_argument {

public:
    explicit InvalidConfigurationException(const std::string& s) : std::invalid_argument{s} {}

    explicit InvalidConfigurationException(const char* s) : invalid_argument(s) {}

    const char* what() const throw() override {
        return logic_error::what();
    }
};

}  // namespace genny


#endif  // HEADER_E4B4F388_CF15_44CC_8782_EA61F79FC2A0_INCLUDED
