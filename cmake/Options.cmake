## Define boolean options and cache variables

option(USE_STATIC_BOOST
       "Use boost static libaries when compiling genny. \
        This also requires boost static libraries to have position-independent code."
       OFF)

option(ENABLE_JASPER
       "Enable the jasper framework and include gRPC."
       ON)

set(GENNY_INSTALL_DIR
    ""
    CACHE PATH "Default install path for genny"
)
