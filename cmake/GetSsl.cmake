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

## Find OpenSSL for both mongo-cxx-driver linking and jasper

# See https://cmake.org/cmake/help/v3.11/module/FindOpenSSL.html
# OPENSSL_ROOT_DIR and OPENSSL_USE_STATIC_LIBS are often important
find_package(OpenSSL 1.1 REQUIRED)

get_filename_component(OPENSSL_CRYPTO_LIBRARY_REALPATH
                       "${OPENSSL_CRYPTO_LIBRARY}" REALPATH CACHE
)
get_filename_component(OPENSSL_SSL_LIBRARY_REALPATH
                       "${OPENSSL_SSL_LIBRARY}" REALPATH CACHE
)
install(FILES
            "${OPENSSL_CRYPTO_LIBRARY_REALPATH}"
            "${OPENSSL_SSL_LIBRARY_REALPATH}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})

