## Set all of the policies we care about for CMake

# https://cmake.org/cmake/help/v3.0/policy/CMP0025.html
if(POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif()

# https://cmake.org/cmake/help/v3.0/ipolicy/CMP0042.html
if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

# https://cmake.org/cmake/help/v3.13/policy/CMP0077.html
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()
