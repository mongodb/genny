set(value_files add_generator.cpp choose_generator.cpp
concatenate_generator.cpp date_generator.cpp increment_generator.cpp
int_or_value.cpp multiply_generator.cpp random_int_generator.cpp
random_string_generator.cpp fast_random_string_generator.cpp value_generator.cpp
use_result_generator.cpp use_value_generator.cpp
use_var_generator.cpp)

include_directories(..)
add_library(values ${value_files})

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
