project(jasperlib VERSION 0.0.1 LANGUAGES CXX)

if(NOT gRPC_FOUND)
    message(WARNING "gRPC not found. Disabling Jasper...")
    set(ENABLE_JASPER OFF CACHE BOOL "" FORCE)
    return()
endif()

add_library(jasperlib STATIC
    jasper.pb.h
    jasper.pb.cc

    jasper.grpc.pb.h
    jasper.grpc.pb.cc
)

target_link_libraries(jasperlib
    PRIVATE
        gRPC::grpc++
)

install(TARGETS  jasperlib
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
