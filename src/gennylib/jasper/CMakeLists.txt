add_library(jasperlib STATIC
    jasper.pb.h
    jasper.pb.cc

    jasper.grpc.pb.h
    jasper.grpc.pb.cc
)

target_include_directories(jasperlib
    PRIVATE
        ${gRPC_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIR}
)

target_link_libraries(jasperlib
    PRIVATE
        ${PROTOBUF_LIBRARIES}
)
