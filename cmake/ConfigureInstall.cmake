# If we did not set the genny install dir, use a local folder
set(GENNY_DEFAULT_INSTALL_DIR "${CMAKE_SOURCE_DIR}/dist")
if(NOT GENNY_INSTALL_DIR)
    set (GENNY_INSTALL_DIR "${GENNY_DEFAULT_INSTALL_DIR}" CACHE PATH "" FORCE)
endif()

# If the user did not customize the install prefix, set it to the genny install dir
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${GENNY_INSTALL_DIR}" CACHE PATH "" FORCE)
endif()

# Add on our install dir since it may be shared with our dependencies
list(APPEND CMAKE_PREFIX_PATH "${GENNY_INSTALL_DIR}")

include(GNUInstallDirs)

set(CMAKE_MACOSX_RPATH                  TRUE                          CACHE BOOL "Ensure that RPATH is used on OSX")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   TRUE                          CACHE BOOL "")
set(CMAKE_INSTALL_RPATH                 "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "")
set(CMAKE_SKIP_INSTALL_RPATH            FALSE                         CACHE BOOL "")
