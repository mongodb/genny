# If the user did not customize the install prefix, set it to our default path
set(GENNY_DEFAULT_INSTALL_DIR "${CMAKE_SOURCE_DIR}/dist")
if(GENNY_INSTALL_DIR)
    set (CMAKE_INSTALL_PREFIX "${GENNY_INSTALL_DIR}" CACHE PATH "" FORCE)
elseif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${GENNY_DEFAULT_INSTALL_DIR}" CACHE PATH "" FORCE)
endif()

# This line allows us to run genny executables from a distribution folder where libraries live in
# bin/../lib64. This is a nice thing for driver-like applications, because you can pass around
# archives trivially.
set(CMAKE_INSTALL_RPATH                 "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"   CACHE PATH "")

# Since we're shooting for redistribution, we change up the RPATH configuration
set(CMAKE_SKIP_BUILD_RPATH              false   CACHE BOOL "")
set(CMAKE_SKIP_INSTALL_RPATH            false   CACHE BOOL "")
set(CMAKE_BUILD_WITH_INSTALL_RPATH      false   CACHE BOOL "")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   true    CACHE BOOL "")
set(CMAKE_MACOSX_RPATH                  ON      CACHE BOOL "Ensure that RPATH is used on OSX")
