add_library(squeezelib
    version.cpp
)

add_executable(main main.cpp)
target_link_libraries(main squeezelib)