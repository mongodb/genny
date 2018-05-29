

# "genny" target
set(genny_files
    version.cpp
)

add_library(genny SHARED ${genny_files})
target_link_libraries(genny PUBLIC ${Boost_LIBRARIES})


# "main" target
set(main_files
    main.cpp
)
add_executable(main ${main_files})
target_link_libraries(main genny)


# subdirectories
add_subdirectory(test)
