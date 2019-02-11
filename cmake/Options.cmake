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

## Define boolean options and cache variables

option(ENABLE_JASPER
       "Enable the jasper framework and include gRPC."
       OFF
)

set(GENNY_INSTALL_DIR
    ""
    CACHE PATH "Default install path for genny"
)

set(GENNY_STATIC_BOOST_PATH
    ""
    CACHE PATH "Use a specific static boost install"
)

option(GENNY_INSTALL_SSL
       "Install the SSL libraries local to the genny install."
       OFF
)

# See http://www.pathname.com/fhs/
option(GENNY_BUILD_FLAT
       "Use FHS-like build directories."
       OFF
)
