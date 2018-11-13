#ifndef HEADER_3B3C3211_A28A_486E_8185_EFA857B22AF6_INCLUDED
#define HEADER_3B3C3211_A28A_486E_8185_EFA857B22AF6_INCLUDED

// Define catch2 helpers here. include this file instead of catch.hpp directly.

#include <cstdio>  /* defines FILENAME_MAX */
#include <unistd.h>

#include <catch.hpp>

inline std::string cwd() {
    char buff[FILENAME_MAX];
    getcwd( buff, FILENAME_MAX );
    std::string current_working_dir(buff);
    return current_working_dir;
}


#endif  // HEADER_3B3C3211_A28A_486E_8185_EFA857B22AF6_INCLUDED
