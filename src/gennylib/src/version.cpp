#include <gennylib/version.hpp>
#include "log.hh"

std::string genny::lib::get_version() {
    BOOST_LOG_TRIVIAL(info) << "Confirmed logging through boost";
    // NB: this version number is duplicated in
    // src/CMakeList.txt, src/gennylib/CMakeLists.txt and src/gennylib/src/version.cpp
    return "0.0.1";
}

