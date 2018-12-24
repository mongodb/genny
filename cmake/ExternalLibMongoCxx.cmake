cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

set(MONGOCXX_PREFIX "${THIRD_PARTY_SOURCE_DIR}/mongo-cxx-driver")

# Use the install prefix and build type for this repo
set(MONGOCXX_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

# Set PIC on
list(APPEND MONGOCXX_CMAKE_ARGS
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
)

# Set some options for the mongo-cxx-driver
list(APPEND MONGOCXX_CMAKE_ARGS
    "-DBSONCXX_POLY_USE_BOOST=1"
#    "-DBUILD_SHARED_LIBS=OFF"
)

ExternalProject_Add(project_libmongocxx EXCLUDE_FROM_ALL
    PREFIX          "${MONGOCXX_PREFIX}"
    DEPENDS         project_libmongoc

    GIT_REPOSITORY  "https://github.com/mongodb/mongo-cxx-driver"
    GIT_TAG         "r3.4.0"

    CMAKE_ARGS      ${MONGOCXX_CMAKE_ARGS}
)
