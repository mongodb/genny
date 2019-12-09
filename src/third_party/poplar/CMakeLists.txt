project(poplarlib VERSION 0.0.1 LANGUAGES CXX)

if(NOT gRPC_FOUND)
    message(FATAL "gRPC not found. Cannot build poplarlib...")
endif()

add_library(poplarlib STATIC
    collector.grpc.pb.cc
    collector.grpc.pb.h
    collector.pb.cc
    collector.pb.h

    metrics.grpc.pb.cc
    metrics.grpc.pb.h
    metrics.pb.cc
    metrics.pb.h

    poplar.grpc.pb.cc
    poplar.grpc.pb.h
    poplar.pb.cc
    poplar.pb.h

    recorder.grpc.pb.cc
    recorder.grpc.pb.h
    recorder.pb.cc
    recorder.pb.h
)

target_link_libraries(poplarlib
    PUBLIC
        gRPC::grpc++
        protobuf
)


install(TARGETS  poplarlib
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
