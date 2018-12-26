## Define boolean options and cache variables

option(USE_STATIC_BOOST
       "Use boost static libaries when compiling genny. \
        This also requires boost static libraries to have position-independent code."
       OFF)

set(GENNY_INSTALL_DIR
    ""
    CACHE PATH "Default install path for genny"
)
