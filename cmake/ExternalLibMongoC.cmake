cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

# Use the install prefix and build type for this repo
set(MONGOC_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

# Set PIC on
list(APPEND MONGOC_CMAKE_ARGS
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
)

# Use our local OpenSSL
list(APPEND MONGOC_CMAKE_ARGS
    "-DOPENSSL_ROOT=${CMAKE_INSTALL_PREFIX}"
)

# Set some options for the mongo-c-driver
list(APPEND MONGOC_CMAKE_ARGS
    "-DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF"
)

ExternalProject_Add(project_libmongoc EXCLUDE_FROM_ALL
    PREFIX          "${THIRD_PARTY_SOURCE_DIR}/mongo-c-driver"

    GIT_REPOSITORY  "https://github.com/mongodb/mongo-c-driver"
    GIT_TAG         "1.13.0"

    CMAKE_ARGS      ${MONGOC_CMAKE_ARGS}
)
