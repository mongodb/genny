message(${CMAKE_CURRENT_LIST_DIR})
find_path(cppcodec_INCLUDE_DIRS "cppcodec/base32_crockford.hpp"
    PATHS "${CMAKE_CURRENT_LIST_DIR}/../src/third_party/cppcodec" # We allow taking the embedded cppcodec
)
mark_as_advanced(cppcodec_INCLUDE_DIRS)

find_package_handle_standard_args(cppcodec
    REQUIRED_VARS cppcodec_INCLUDE_DIRS
)

if(cppcodec_FOUND)
    add_library(cppcodec::cppcodec IMPORTED INTERFACE)
    target_include_directories(cppcodec::cppcodec INTERFACE "${cppcodec_INCLUDE_DIRS}")
endif()
