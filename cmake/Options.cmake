## Define boolean options and cache variables

option(ENABLE_JASPER
       "Enable the jasper framework and include gRPC."
       OFF
)

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
