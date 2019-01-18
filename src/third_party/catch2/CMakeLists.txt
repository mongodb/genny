
add_library(catch2 SHARED ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_include_directories(catch2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

install(TARGETS  catch2
        EXPORT   GennyLibraryConfig
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows


include(ParseAndAddCatchTests.cmake)
set(AdditionalCatchParameters --reporter junit --durations yes --rng-seed 12345 --success)
