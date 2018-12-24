cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

set(OPENSSL_PREFIX "${THIRD_PARTY_SOURCE_DIR}/openssl")

# Use the install prefix and build type for this repo
set(OPENSSL_CONFIGURE_COMMAND
    "/bin/bash"
    "${OPENSSL_PREFIX}/src/openssl/config"
    "--prefix=${CMAKE_INSTALL_PREFIX}"
    "--openssldir=${CMAKE_INSTALL_PREFIX}"
)

# Turn off older options
list(APPEND OPENSSL_CONFIGURE_COMMAND
    no-shared
    no-ssl2
    no-ssl3
)

# Set PIC on
list(APPEND OPENSSL_CONFIGURE_COMMAND
    "-fPIC"
)

ExternalProject_Add(openssl EXCLUDE_FROM_ALL
    PREFIX          "${OPENSSL_PREFIX}"

    GIT_REPOSITORY  "https://github.com/openssl/openssl"
    GIT_TAG         "OpenSSL_1_1_1"

    CONFIGURE_COMMAND   "${OPENSSL_CONFIGURE_COMMAND}"
)
