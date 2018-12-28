## Define boolean options and cache variables

option(USE_STATIC_BOOST
       "Use boost static libaries when compiling genny. \
        This also requires boost static libraries to have position-independent code."
       OFF)

option(ENABLE_SSL
       "Use SSL for mongo-c-driver and jasperlib."
       ON)

option(ENABLE_JASPER
       "Enable the jasper framework and include gRPC."
       ON)

if(NOT ENABLE_SSL AND ENABLE_JASPER)
    message(FATAL "SSL is required for Jasper.")
endif()

set(GENNY_INSTALL_DIR
    ""
    CACHE PATH "Default install path for genny"
)
