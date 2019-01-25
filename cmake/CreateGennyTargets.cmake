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

##
# Defines the CreateGennyTargets function
##

#
# Usage:
#
#     CreateGennyTargets(
#       NAME   project_name
#       TYPE   SHARED|STATIC|INTERFACE
#       DEPENDS
#         list of other libs to link to for output lib
#       TEST_DEPENDS
#         list of other libs to link to for test binary
#       EXECUTABLE name of file to produce for src/main.cpp
#     )
#
# Notes:
#
# -   Assumes public headers are in `include` and end with `.hpp`
# -   Assumes source files are in `src` and end with `.cpp`.
# -   The file `src/main.cpp` is special. It is not compiled into the
#     produced library. It is only compiled if the `EXECUTABLE` argument
#     is given.
# -   Assumes regular tests are `test/*.cpp` and get parsed by
#     `ParseAndAddCatchTests`. The test binary is named
#     `project_name_test`.
# -   Assumes benchmark tests are `benchmark/*.cpp` and get parsed by
#     `ParseAndAddCatchTests`. The benchmark test binary is named
#     `project_name_benchmark`.
# -   Installs the library to the `GennyLibraryConfig` export
#
function(CreateGennyTargets)
    set(options)
    set(oneValueArgs NAME TYPE EXECUTABLE)
    set(multiValueArgs DEPENDS TEST_DEPENDS)
    cmake_parse_arguments(CGT "${options}" "${oneValueArgs}"
            "${multiValueArgs}" ${ARGN})

    ## business-logic to compute linkage based on TYPE=_gs_type

    # normally we want
    #   target_include_directories(name PUBLIC ...)
    # for both SHARED and STATIC types
    # but for type INTERFACE we want
    #   target_include_directories(name INTERFACE ...)
    set(_gs_include_type "PUBLIC")
    if(CGT_TYPE MATCHES "^INTERFACE$")
        set(_gs_include_type "INTERFACE")
    endif()

    # set _gs_private_src "PRIVATE;src" (list)
    # if type isn't INTERFACES. This lets
    # e.g. value_generators #include <> files
    # in the src direcory. Cannot do this for
    # INTERFACE targets.
    set(_gs_private_src)
    if(NOT CGT_TYPE MATCHES "^INTERFACE$")
        list(APPEND _gs_private_src PRIVATE)
        list(APPEND _gs_private_src src)
    endif()

    ## Apply conventions for files in targets

    # the CONFIGURE_DEPENDS here means we re-run the
    # glob command at `make` time so we automatically
    # find new files without having to re-run `cmake`.

    # _gs_files_src = src/*.cpp
    file(GLOB_RECURSE _gs_files_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         src/*.cpp)
    # remove main.cpp, it must be added via EXECUTABLE
    list(REMOVE_ITEM _gs_files_src "src/main.cpp")

    # _gs_tests_src = test/*.cpp
    file(GLOB_RECURSE _gs_tests_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         test/*.cpp)

    # _gs_benchmark_src = benchmark/*.cpp
    file(GLOB_RECURSE _gs_benchmarks_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         benchmark/*.cpp)

    ## create library

    add_library("${CGT_NAME}" "${CGT_TYPE}" ${_gs_files_src})

    target_include_directories("${CGT_NAME}"
        "${_gs_include_type}"
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
         ${_gs_private_src}
    )

    target_link_libraries("${CGT_NAME}"
        "${_gs_include_type}"
            ${CGT_DEPENDS}
    )

    ## install / put in export

    install(DIRECTORY include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN *.hpp)

    install(TARGETS  "${CGT_NAME}"
            EXPORT   GennyLibraryConfig
            ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows

    ## add EXECUTABLE from src/main.cpp

    if(CGT_EXECUTABLE)
        add_executable("${CGT_EXECUTABLE}" src/main.cpp)
        target_link_libraries("${CGT_EXECUTABLE}" "${CGT_NAME}")
        install(TARGETS  "${CGT_EXECUTABLE}"
                RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()


    ## regular test

    if (_gs_tests_src) # if any tests
        add_executable("${CGT_NAME}_test"
            ${_gs_tests_src}
        )
        target_link_libraries("${CGT_NAME}_test"
            "${CGT_NAME}"
            ${CGT_TEST_DEPENDS}
        )
        ParseAndAddCatchTests("${CGT_NAME}_test")
    endif()

    ## benchmark test

    if(_gs_benchmarks_src) # if any benchmark files
        add_executable("${CGT_NAME}_benchmark"
            ${_gs_benchmarks_src}
        )
        target_link_libraries("${CGT_NAME}_benchmark"
            "${CGT_NAME}"
            ${CGT_TEST_DEPENDS}
        )
        ParseAndAddCatchTests("${CGT_NAME}_benchmark")
    endif()

endfunction(CreateGennyTargets)
