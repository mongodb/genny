add_library(jasperlib STATIC
    jasper.pb.h
    jasper.pb.cc

    jasper.grpc.pb.h
    jasper.grpc.pb.cc
)

find_library(GRPCXX grpc++)
find_library(PROTOBUF protobuf)

target_link_libraries(jasperlib
    PRIVATE
        ${GRPCXX}
        ${PROTOBUF}
)
