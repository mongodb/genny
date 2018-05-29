
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(
    Boost
    1.67
    REQUIRED
    COMPONENTS
    log_setup
    log
)

include_directories("${Boost_INCLUDE_DIRS}")

add_subdirectory(squeeze)
