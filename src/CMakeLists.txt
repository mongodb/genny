
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
include_directories("${Boost_INCLUDE_DIRS}")
# </Boost>

# So we can #include <genny/foo.h>
include_directories(.)

add_subdirectory(genny)
