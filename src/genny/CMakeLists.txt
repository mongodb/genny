

# "genny" library
set(genny_files
    version.cpp
)

# "main" target
set(main_files
    main.cpp
)

add_library(genny ${genny_files})
target_link_libraries(genny
    PUBLIC ${Boost_LIBRARIES}
)

add_executable(main ${main_files})
target_link_libraries(main
    PUBLIC genny
)


# subdirectories
add_subdirectory(test)
