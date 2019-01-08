## Define boolean options and cache variables

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

set(GENNY_STATIC_BOOST_PATH
    ""
    CACHE PATH "Use a specific static boost install"
)

option(GENNY_INSTALL_SSL
       "Install the SSL libraries local to the genny install."
       OFF
)
