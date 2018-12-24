# <Boost>
set(Boost_USE_STATIC_LIBS ${USE_STATIC_BOOST})
if(NOT ${USE_STATIC_BOOST})
  add_definitions(-DBOOST_ALL_DYN_LINK)
endif()

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(
        Boost
        1.58 # Minimum version, vetted via Ubuntu 16.04
        REQUIRED
        COMPONENTS
        log_setup
        log
        program_options
)
#add_library(boost INTERFACE IMPORTED)
#set_property(TARGET boost PROPERTY
#             INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
# </Boost>
