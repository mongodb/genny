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

project(value_generators VERSION 0.0.1 LANGUAGES CXX)

add_library(value_generators STATIC
    src/generators-private.cpp
    src/parser.cpp
    src/value_generators.cpp
)
target_include_directories(value_generators
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(value_generators
    MongoCxx::bsoncxx
    yaml-cpp
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp)

install(TARGETS  value_generators
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows


add_executable(value_generators_test
    test/document_test.cpp
    test/DefaultRandom_test.cpp
    $<TARGET_OBJECTS:catch2>
)

target_link_libraries(value_generators_test gennylib_testutil)

ParseAndAddCatchTests(value_generators_test)

