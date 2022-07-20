# <Catch2>
add_subdirectory(catch2)
# </Catch2>
IF(NOT TARGET Catch2::Catch2)
    message(MESSAGE "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX NOT TARGET Catch2::Catch2")
ENDIF()

# <Loki>
add_subdirectory(loki)
# </Loki>

# <gRPC>
include(GetGRpc)
# </gRPC>

add_subdirectory(poplar)

add_subdirectory(cppcodec)
add_subdirectory(simple-beast-client)
