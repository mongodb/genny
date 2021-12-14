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

find_package(libmongocxx REQUIRED)

# Make a target for bsoncxx
add_library(MongoCxx::bsoncxx SHARED IMPORTED GLOBAL)
set_target_properties(MongoCxx::bsoncxx
    PROPERTIES
        IMPORTED_LOCATION   "${LIBBSONCXX_LIBRARY_PATH}"
)
target_include_directories(MongoCxx::bsoncxx
    INTERFACE
        ${LIBBSONCXX_INCLUDE_DIRS}
)

# Make a target for mongocxx
add_library(MongoCxx::mongocxx SHARED IMPORTED GLOBAL)
set_target_properties(MongoCxx::mongocxx
    PROPERTIES
        IMPORTED_LOCATION   "${LIBMONGOCXX_LIBRARY_PATH}"
)
target_include_directories(MongoCxx::mongocxx
    INTERFACE
        ${LIBMONGOCXX_INCLUDE_DIRS}
)
target_link_libraries(MongoCxx::mongocxx
    INTERFACE
        MongoCxx::bsoncxx
)
