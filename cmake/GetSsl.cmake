## Find OpenSSL for both mongo-cxx-driver linking and jasper

# See https://cmake.org/cmake/help/v3.11/module/FindOpenSSL.html
# OPENSSL_ROOT_DIR and OPENSSL_USE_STATIC_LIBS are often important
find_package(OpenSSL 1.1 REQUIRED)

if(GENNY_INSTALL_SSL)
    get_filename_component(OPENSSL_CRYPTO_LIBRARY_REALPATH
                           "${OPENSSL_CRYPTO_LIBRARY}" REALPATH CACHE
    )
    get_filename_component(OPENSSL_SSL_LIBRARY_REALPATH
                           "${OPENSSL_SSL_LIBRARY}" REALPATH CACHE
    )
    install(FILES
                "${OPENSSL_CRYPTO_LIBRARY_REALPATH}"
                "${OPENSSL_SSL_LIBRARY_REALPATH}"
            DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
