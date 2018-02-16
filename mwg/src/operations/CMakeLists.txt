set(src_files find.cpp insert_many.cpp insert_one.cpp count.cpp
delete_one.cpp delete_many.cpp create_index.cpp distinct.cpp drop.cpp
find_one.cpp find_one_and_update.cpp find_one_and_replace.cpp
find_one_and_delete.cpp replace_one.cpp list_indexes.cpp
read_preference.cpp write_concern.cpp name.cpp
update_one.cpp update_many.cpp create_collection.cpp command.cpp
set_variable.cpp noop.cpp)
add_library(operations ${src_files})
include_directories(documents nodes operations values)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_BINARY_DIR})
