add_library(lib_squeeze
    version.cpp
)

add_executable(main main.cpp)
target_link_libraries(main lib_squeeze)

add_subdirectory(test)