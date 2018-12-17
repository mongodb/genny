project(cast_core VERSION 0.0.1 LANGUAGES CXX)

# Pacify the CLion
file(GLOB_RECURSE HEADERS_FOR_CLION
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     CONFIGURE_DEPENDS
     include/*.hpp include/*.hh src/*.hh)
add_library(cast_core_headers SHARED
    EXCLUDE_FROM_ALL
    src/cast_core.cpp
    ${HEADERS_FOR_CLION}
)

add_library(cast_core SHARED
    # ActorsStart
    src/actors/HelloWorld.cpp
    src/actors/Insert.cpp
    src/actors/InsertRemove.cpp
    src/actors/Loader.cpp
    src/actors/MultiCollectionQuery.cpp
    src/actors/MultiCollectionUpdate.cpp
    # ActorsEnd
)

target_include_directories(cast_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
)

target_link_libraries(cast_core
    PUBLIC
        gennylib
    PRIVATE
        gennylib_cast
        gennylib_generators
)

install(TARGETS  cast_core EXPORT GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp)
