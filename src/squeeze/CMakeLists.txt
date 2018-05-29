link_libraries(${Boost_LIBRARIES})

add_library(lib_squeeze
    version.cpp
)

add_executable(main main.cpp)

target_include_directories(main PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(main lib_squeeze ${Boost_LOG_LIBRARY})
#target_link_libraries(main LINK_PUBLIC )

add_subdirectory(test)

