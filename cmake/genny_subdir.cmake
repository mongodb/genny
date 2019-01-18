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

    set(_gs_depends)
    set(_gs_test_depends)
    set(_gs_list_target "")

    foreach(arg IN LISTS ARGV)
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

    # debug("Deduced name=${_gs_name} type=${_gs_type} depends=${_gs_depends} test_depends=${_gs_test_depends}")

    # TODO: is this glob right
    file(GLOB_RECURSE _gs_files_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         src/*.cpp)
    # TODO: is this glob right
    file(GLOB_RECURSE _gs_tests_src
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
         CONFIGURE_DEPENDS
         test/*.cpp)

    add_library("${_gs_name}" "${_gs_type}" ${_gs_files_src})
    target_include_directories(${_gs_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries("${_gs_name}"
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

    add_executable("${_gs_name}_test"
        ${_gs_tests_src}
    )

    target_link_libraries("${_gs_name}_test"
        "${_gs_name}"
        ${_gs_test_depends}
    )

    ParseAndAddCatchTests("${_gs_name}_test")
endfunction(GENNY_SUBDIR)
