# =============================================================================
# Third-party dependencies — one-stop setup for all vendored / prebuilt libs.
# Included from the root CMakeLists.txt before add_subdirectory(common).
#
# Directory layout under 3rd/:
#   source/    — third-party source distributions
#   prebuilt/  — pre-compiled binary packages (currently only OpenSSL)
# =============================================================================

get_filename_component(_repo_root ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
set(_3rd_source   ${_repo_root}/3rd/source)
set(_3rd_prebuilt ${_repo_root}/3rd/prebuilt)

# ── spdlog ──────────────────────────────────────────────────────────────────
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
if(NOT TARGET spdlog)
    add_subdirectory(${_3rd_source}/spdlog-1.17.0
                     ${CMAKE_BINARY_DIR}/3rd/spdlog)
endif()

# ── yaml-cpp ────────────────────────────────────────────────────────────────
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
if(NOT TARGET yaml-cpp)
    add_subdirectory(${_3rd_source}/yaml-cpp-0.9.0
                     ${CMAKE_BINARY_DIR}/3rd/yaml-cpp)
    set_target_properties(yaml-cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ── OpenSSL ─────────────────────────────────────────────────────────────────
# Prebuilts under 3rd/lib/openssl/<platform>/.  If missing, build from
# vendored source (one-time), then install there.  The lib directory is
# version-controlled so other machines can clone and build immediately.
#
# MSVC:  scripts/build_openssl_msvc.bat  (perl Configure VC-WIN64A + nmake)
# Linux: scripts/build_openssl.sh        (./Configure + make)

list(PREPEND CMAKE_MODULE_PATH "${_repo_root}/cmake")

if(NOT TARGET OpenSSL::SSL)
    if(MSVC)
        set(_ssl_prefix "${_repo_root}/3rd/lib/openssl/MSVC")
        set(_ssl_lib    "${_ssl_prefix}/lib/libssl.lib")
        set(_crypto_lib "${_ssl_prefix}/lib/libcrypto.lib")
        set(_build_cmd  "${_repo_root}/scripts/build_openssl_msvc.bat")
    else()
        set(_ssl_prefix "${_repo_root}/3rd/lib/openssl/Linux")
        set(_ssl_lib    "${_ssl_prefix}/lib/libssl.a")
        set(_crypto_lib "${_ssl_prefix}/lib/libcrypto.a")
        set(_build_cmd  "${_repo_root}/scripts/build_openssl.sh")
    endif()
    set(_inc_dir "${_ssl_prefix}/include")

    if(NOT EXISTS "${_ssl_lib}")
        message(STATUS "Building OpenSSL from source...")
        if(MSVC)
            execute_process(
                COMMAND "${_build_cmd}" "${_ssl_prefix}"
                WORKING_DIRECTORY "${_repo_root}"
                RESULT_VARIABLE _r)
        else()
            execute_process(
                COMMAND bash "${_build_cmd}" "${_ssl_prefix}"
                WORKING_DIRECTORY "${_repo_root}"
                RESULT_VARIABLE _r)
        endif()
        if(NOT _r EQUAL 0)
            message(FATAL_ERROR "OpenSSL build failed (exit ${_r})")
        endif()
        message(STATUS "OpenSSL build complete")
    endif()

    add_library(OpenSSL_SSL STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL_SSL PROPERTIES
        IMPORTED_LOCATION "${_ssl_lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${_inc_dir}")
    add_library(OpenSSL_Crypto STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL_Crypto PROPERTIES
        IMPORTED_LOCATION "${_crypto_lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${_inc_dir}")

    add_library(OpenSSL::SSL    ALIAS OpenSSL_SSL)
    add_library(OpenSSL::Crypto ALIAS OpenSSL_Crypto)
    set(OPENSSL_FOUND TRUE CACHE BOOL "" FORCE)

    # Fallback for curl's find_package if our shim doesn't intercept
    set(OPENSSL_ROOT_DIR "${_ssl_prefix}" CACHE PATH "" FORCE)
endif()

# ── curl ────────────────────────────────────────────────────────────────────
if(NOT TARGET libcurl)
    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
    set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    # Our OpenSSL is built with no-srp; disable in curl too.
    set(CURL_DISABLE_SRP ON CACHE BOOL "" FORCE)
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)

    # Block Perl detection so curl skips add_subdirectory(docs) —
    # docs/ was deleted from vendored source to save ~5M.
    set(CMAKE_DISABLE_FIND_PACKAGE_Perl ON CACHE BOOL "" FORCE)
    add_subdirectory(${_3rd_source}/curl-8.21.0
                     ${CMAKE_BINARY_DIR}/3rd/curl)
endif()
