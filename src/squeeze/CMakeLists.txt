

# "squeeze" target
set(squeeze_files
    version.cpp
)

add_library(squeeze SHARED ${squeeze_files})
target_link_libraries(squeeze PUBLIC ${Boost_LIBRARIES})


# "main" target
set(main_files
    main.cpp
)
add_executable(main ${main_files})
target_link_libraries(main squeeze)


# subdirectories
add_subdirectory(test)
