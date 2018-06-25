# Define an executable
add_executable(genny
    src/main.cpp
    src/DefaultDriver.cpp src/DefaultDriver.hpp
)

# Define the libraries this project depends upon
target_link_libraries(genny gennylib)
