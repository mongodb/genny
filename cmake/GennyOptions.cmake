option(INSTALL_THIRD_PARTY
"Download and install openssl, mongo-c-driver, mongo-cxx-driver, and grpc. \
This may or may not be useful to those who build from source. \
It is definitely useful for creating packages for distribution."
        OFF)

option(USE_STATIC_BOOST
"Use boost static libaries when compiling genny. \
This also requires boost static libraries to have position-independent code."
       OFF)
