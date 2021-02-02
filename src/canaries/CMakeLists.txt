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

project(canaries VERSION 0.0.1 LANGUAGES CXX)

CreateGennyTargets(
    NAME    canaries
    TYPE    STATIC
    DEPENDS
        gennylib
        metrics
        Boost::program_options
        Boost::filesystem
    TEST_DEPENDS  testlib
    EXECUTABLE    genny-canaries
)
