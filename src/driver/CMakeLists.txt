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

project(driver VERSION 0.0.1 LANGUAGES CXX)

# Define an underlying core compilation unit
add_library(driver STATIC
    src/DefaultDriver.cpp
)
target_include_directories(driver
    PUBLIC
        src
)
target_link_libraries(driver
    cast_core
    gennylib

    Boost::program_options
)

# Make the genny driver executable
add_executable(genny src/main.cpp)
target_link_libraries(genny driver)
install(TARGETS  genny
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})

# Define our test for the driver
add_executable(test_driver
    test/Driver_test.cpp
    $<TARGET_OBJECTS:catch2>
)
target_include_directories(test_driver
    PRIVATE
        src
)
target_link_libraries(test_driver driver gennylib_testutil)

ParseAndAddCatchTests(test_driver)

# Install all the workload documents
file(GLOB WORKLOADS
     test/*.yml
     test/*.yaml
)
install(FILES ${WORKLOADS}
        DESTINATION etc/genny/workloads
)
