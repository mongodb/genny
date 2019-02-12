# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

# This line allows us to run genny executables from a distribution folder where libraries live in
# bin/../lib64. This is a nice thing for driver-like applications, because you can pass around
# archives trivially.
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}" "@loader_path/../${CMAKE_INSTALL_LIBDIR}" CACHE PATH "")

# Since we're shooting for redistribution, we change up the RPATH configuration
set(CMAKE_SKIP_BUILD_RPATH              false   CACHE BOOL "")
set(CMAKE_SKIP_INSTALL_RPATH            false   CACHE BOOL "")
set(CMAKE_BUILD_WITH_INSTALL_RPATH      false   CACHE BOOL "")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH   true    CACHE BOOL "")
set(CMAKE_MACOSX_RPATH                  ON      CACHE BOOL "Ensure that RPATH is used on OSX")
