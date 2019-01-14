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
