#include <gennylib/version.h>
#include "log.h"

std::string genny::lib::get_version() {
    BOOST_LOG_TRIVIAL(info) << "Confirmed logging through boost";
    return "0.0.1";
}

