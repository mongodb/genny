## Find OpenSSL for both mongo-cxx-driver linking and jasper

# See https://cmake.org/cmake/help/v3.11/module/FindOpenSSL.html
# OPENSSL_ROOT_DIR and OPENSSL_USE_STATIC_LIBS are often important
find_package(OpenSSL 1.1 REQUIRED)
