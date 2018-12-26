# <yaml-cpp>
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "No tests for YAML" FORCE)
add_subdirectory(yaml-cpp)
# </yaml-cpp>

# <Catch2>
add_subdirectory(catch2)
# </Catch2>

# <Jasper>
add_subdirectory(jasper)
# </Jasper>
