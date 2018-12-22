project(jasperlib VERSION 0.0.1 LANGUAGES CXX)

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
