# <Loki>
add_subdirectory(loki)
# </Loki>

# <gRPC>
include(GetGRpc)
# </gRPC>

add_subdirectory(poplar)

add_subdirectory(cppcodec)
add_subdirectory(simple-beast-client)
