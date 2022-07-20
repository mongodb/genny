# <Loki>
add_subdirectory(loki)
# </Loki>

# <gRPC>
include(GetGRpc)
# </gRPC>

add_subdirectory(poplar)

# <Catch2>
add_subdirectory(catch2)
# </Catch2>

add_subdirectory(cppcodec)
add_subdirectory(simple-beast-client)
