# Shim — curl calls find_package(OpenSSL REQUIRED) which resets
# OPENSSL_FOUND to FALSE before searching.  Intercept and return
# immediately when our imported targets already exist.
if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
    set(OPENSSL_FOUND TRUE)
    return()
endif()

# Fallback: let CMake's built-in FindOpenSSL handle it
include(${CMAKE_ROOT}/Modules/FindOpenSSL.cmake)
