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

add_subdirectory(third_party)

add_subdirectory(gennylib)
add_subdirectory(cast_core)
add_subdirectory(driver)

# This makes the project importable from the build directory
export(TARGETS gennylib gennylib_cast gennylib_generators gennylib_core loki cast_core
       FILE GennyLibraryConfig.cmake)

# Below generates a cmake module so if you make install from this
# project you can find_package(GennyLib) in another cmake project.
#
# Please don't build any projects that link against gennylib outside of
# this repository please. Not yet. We need to think of a good ABI.
# We're only exposing external cmake bindings because it encourages
# best-practices and helps keep the codebase honest about what it
# exports and how it's used.
install(EXPORT GennyLibraryConfig DESTINATION share/GennyLibrary/cmake)
