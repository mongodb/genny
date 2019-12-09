project(poplarlib VERSION 0.0.1 LANGUAGES CXX)

if(NOT gRPC_FOUND)
    message(FATAL "gRPC not found. Cannot build poplarlib...")
endif()

add_library(poplarlib STATIC
    poplarlib/collector.grpc.pb.cc
    poplarlib/collector.grpc.pb.h
    poplarlib/collector.pb.cc
    poplarlib/collector.pb.h

    poplarlib/metrics.grpc.pb.cc
    poplarlib/metrics.grpc.pb.h
    poplarlib/metrics.pb.cc
    poplarlib/metrics.pb.h

    poplarlib/poplar.grpc.pb.cc
    poplarlib/poplar.grpc.pb.h
    poplarlib/poplar.pb.cc
    poplarlib/poplar.pb.h

    poplarlib/recorder.grpc.pb.cc
    poplarlib/recorder.grpc.pb.h
    poplarlib/recorder.pb.cc
    poplarlib/recorder.pb.h
)

target_link_libraries(poplarlib
    PUBLIC
        gRPC::grpc++
        protobuf
)

target_include_directories(poplarlib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS  poplarlib
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
