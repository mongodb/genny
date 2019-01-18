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

project(metrics VERSION 0.0.1 LANGUAGES CXX)

# For more info on interface libraries,
# see https://cmake.org/cmake/help/v3.5/command/add_library.html#interface-libraries
add_library(metrics INTERFACE)
target_include_directories(metrics
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        )

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp)

install(TARGETS  metrics
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows


add_executable(test_metrics
    test/metrics_test.cpp
)

target_link_libraries(test_metrics metrics catch2 gennylib_testutil)

ParseAndAddCatchTests(test_metrics)

