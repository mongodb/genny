cmake_minimum_required(VERSION 3.10)
project(simple-beast-client VERSION 0.0.1 LANGUAGES CXX)

add_library(simple-beast-client INTERFACE)
find_package(Boost 1.66 REQUIRED COMPONENTS
               date_time regex)
find_package(OpenSSL)
find_package(Threads)
find_package(cppcodec)
# SYSTEM INTERFACE means we don't emit warnings.
target_include_directories(simple-beast-client SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(simple-beast-client INTERFACE
    ENABLE_DIGEST
    ENABLE_HTTPS)

install(DIRECTORY include
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN *.hpp)

install(TARGETS  simple-beast-client
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows

# example program is useful for validating the library
#add_subdirectory(example)
