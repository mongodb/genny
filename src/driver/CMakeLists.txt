# Define an executable
add_executable(genny
    src/main.cpp
)

# Define the libraries this project depends upon
target_link_libraries(genny gennylib)

target_compile_options(genny
    PRIVATE
        -Wall -Wextra -Werror
)