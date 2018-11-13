# Define an executable
add_executable(genny
    src/main.cpp
    src/DefaultDriver.cpp
)

# Define the libraries this project depends upon
target_link_libraries(genny gennylib)

install(TARGETS  genny
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})
