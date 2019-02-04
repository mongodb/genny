find_package(libmongocxx-static REQUIRED)
find_package(libmongoc-static-1.0 CONFIG REQUIRED)

# Make a target for bsoncxx
add_library(MongoCxx::bsoncxx STATIC IMPORTED GLOBAL)
set_target_properties(MongoCxx::bsoncxx
    PROPERTIES
        IMPORTED_LOCATION   "${LIBBSONCXX_STATIC_LIBRARY_PATH}"
)
target_include_directories(MongoCxx::bsoncxx
    INTERFACE
        ${LIBMONGOCXX_STATIC_INCLUDE_DIRS}
)

target_link_libraries(MongoCxx::bsoncxx
    INTERFACE
        ${MONGOC_STATIC_LIBRARIES}
)

# Make a target for mongocxx
add_library(MongoCxx::mongocxx STATIC IMPORTED GLOBAL)
set_target_properties(MongoCxx::mongocxx
    PROPERTIES
        IMPORTED_LOCATION   "${LIBMONGOCXX_STATIC_LIBRARY_PATH}"
)
target_include_directories(MongoCxx::mongocxx
    INTERFACE
        ${LIBMONGOCXX_STATIC_INCLUDE_DIRS}
)
target_link_libraries(MongoCxx::mongocxx
    INTERFACE
        MongoCxx::bsoncxx
)
