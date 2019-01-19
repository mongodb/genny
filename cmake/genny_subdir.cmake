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

macro(debug msg)
    message("DEBUG ${msg}")
endmacro()

macro(debugValue variableName)
    debug("${variableName} = \${${variableName}}")
endmacro()

function(GENNY_SUBDIR)
    set(_gs_name)
    set(_gs_want_name 0)

    set(_gs_type)
    set(_gs_want_type 0)

    set(_gs_executable)
    set(_gs_want_executable 0)

    set(_gs_depends)
    set(_gs_test_depends)
    set(_gs_list_target "")

    # for each arg we look for caps keywords
    # e.g. NAME and set the appropriate _gs_want_X
    # var. Then on next iteration if we have the
    # _gs_want_X we set the _gs_X to the current value
    # and unset the _gs_want var. For list types
    # e.g. DEPENDS/TEST_DEPENDS we append to the
    # appropriate lists.
    foreach(arg IN LISTS ARGV)
        # we could be more pedantic in here
        # to assert that we have at most
        # one _gs_want* set but that seems..icky.
        if(arg MATCHES "^NAME$")
            set(_gs_want_name 1)
            continue()
        elseif(_gs_want_name)
            set(_gs_name "${arg}")
            set(_gs_want_name 0)
            continue()
        elseif(arg MATCHES "^TYPE$")
            set(_gs_want_type 1)
            continue()
        elseif(_gs_want_type)
            set(_gs_type "${arg}")
            set(_gs_want_type 0)
            continue()
        elseif(arg MATCHES "^EXECUTABLE")
            set(_gs_want_executable 1)
            continue()
        elseif(_gs_want_executable)
            set(_gs_executable "${arg}")
            set(_gs_want_executable 0)
            continue()
        elseif(arg MATCHES "^DEPENDS$")
            set(_gs_list_target "_gs_depends")
            continue()
        elseif(arg MATCHES "^TEST_DEPENDS$")
            set(_gs_list_target "_gs_test_depends")
            continue()
        else()
            list(APPEND "${_gs_list_target}" "${arg}")
        endif()
    endforeach()

    # normally we want
    #   target_include_directories(name PUBLIC ...)
    # for both PUBLIC and STATIC types
    # but for type INTERFACE we want
    #   target_include_directories(name INTERFACE ...)
    set(_gs_include_type "PUBLIC")
    if(_gs_type MATCHES "^INTERFACE$")
        set(_gs_include_type "INTERFACE")
    endif()

    # set _gs_private_src "PRIVATE;src" (list)
    # if type isn't INTERFACES. This lets
    # e.g. value_generators #include <> files
    # in the src direcory. Cannot do this for
    # INTERFACE targets.
    set(_gs_private_src)
    if(NOT _gs_type MATCHES "^INTERFACE$")
        list(APPEND _gs_private_src PRIVATE)
        list(APPEND _gs_private_src src)
    endif()

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

    # _gs_benchmark_src
    file(GLOB_RECURSE _gs_benchmarks_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         benchmark/*.cpp)

    add_library("${_gs_name}" "${_gs_type}" ${_gs_files_src})
    target_include_directories(${_gs_name}
        "${_gs_include_type}"
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
         ${_gs_private_src}
    )

    target_link_libraries("${_gs_name}"
        "${_gs_include_type}"
            ${_gs_depends}
    )

    install(DIRECTORY include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN *.hpp)

    install(TARGETS  "${_gs_name}"
            EXPORT   GennyLibraryConfig
            ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows

    if(_gs_executable)
        add_executable("${_gs_executable}" src/main.cpp)
        target_link_libraries("${_gs_executable}" "${_gs_name}")
        install(TARGETS  "${_gs_executable}"
                RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()


    add_executable("${_gs_name}_test"
        ${_gs_tests_src}
    )
    target_link_libraries("${_gs_name}_test"
        "${_gs_name}"
        ${_gs_test_depends}
    )
    ParseAndAddCatchTests("${_gs_name}_test")

    if(_gs_benchmarks_src)
        add_executable("${_gs_name}_benchmark"
            ${_gs_benchmarks_src}
        )
        target_link_libraries("${_gs_name}_benchmark"
            "${_gs_name}"
            ${_gs_test_depends}
        )
        ParseAndAddCatchTests("${_gs_name}_benchmark")
    endif()

endfunction(GENNY_SUBDIR)
