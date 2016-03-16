set(value_files date_generator.cpp increment_generator.cpp
multiply_generator.cpp random_int_generator.cpp
random_string_generator.cpp value_generator.cpp
use_result_generator.cpp use_value_generator.cpp
use_var_generator.cpp)

include_directories(..)
add_library(values ${value_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
