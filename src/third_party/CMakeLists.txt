# <yaml-cpp>
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "No tests for YAML" FORCE)
add_subdirectory(yaml-cpp)
# </yaml-cpp>

# <Catch2>
add_subdirectory(catch2)
# </Catch2>

# <Loki>
add_subdirectory(loki)
# </Loki>

# <Mongo Driver>
# Use our FindMongoCxx module to wrap the mongo-cxx-driver in a target
find_package(MongoCxx)
# </Mongo Driver>

# <Jasper>
if(ENABLE_JASPER)
    add_subdirectory(jasper)
endif()
# </Jasper>
