
# don't look in system paths for boost, use our own location
#set(Boost_NO_SYSTEM_PATHS ON)

# not ideal but this is where brew installs boost and
# it's easy to set --prefix=/usr/local/opt/boost when
# installing manually.
#set(BOOST_ROOT /usr/local/opt/boost)
#set(Boost_USE_STATIC_LIBS OFF)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_LOG_DYN_LINK")

set( Boost_DEBUG        ON )

find_package(
    Boost
    1.67
    REQUIRED
    COMPONENTS
    log
    log_setup
)

add_subdirectory(squeeze)
