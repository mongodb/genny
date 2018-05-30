
# <Boost>
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(
    Boost
    1.58 # only 1.58 because that's the version apt-get installs on ubuntu 16.04
    REQUIRED
    COMPONENTS
    log_setup
    log
)
# </Boost>

set(THIRD_PARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# so you can #import <genny/foo.h>
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

add_subdirectory(genny)
