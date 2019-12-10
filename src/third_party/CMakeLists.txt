# <Loki>
add_subdirectory(loki)
# </Loki>

# <Mongo Driver>
# Use our FindMongoCxx module to wrap the mongo-cxx-driver in a target
find_package(MongoCxx)
# </Mongo Driver>

# <gRPC>
include(GetGRpc)
# </gRPC>

add_subdirectory(poplar)

# <Jasper>
if(ENABLE_JASPER)
    add_subdirectory(jasper)
endif()
# </Jasper>

# <Catch2>
add_subdirectory(catch2)
# </Catch2>