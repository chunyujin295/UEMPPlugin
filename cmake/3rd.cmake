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
# Windows: prebuilt binaries (built once via scripts/build_openssl_*)
# Linux:   system-installed (apt install libssl-dev or equivalent)
if(NOT TARGET OpenSSL::SSL)
    # --- Windows: direct IMPORTED GLOBAL creation ---
    # Bypasses CMake's FindOpenSSL which has incompatible behavior across
    # CMake 3.x / 4.x (ALIAS vs direct IMPORTED, scope visibility).
    if(MINGW OR MSVC)
        if(MINGW)
            set(OPENSSL_ROOT_DIR "${_3rd_prebuilt}/openssl/mingw"
                CACHE PATH "OpenSSL prebuilt (MinGW)" FORCE)
            set(_ssl_lib    "${OPENSSL_ROOT_DIR}/lib/libssl.a")
            set(_crypto_lib "${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
        elseif(MSVC)
            set(OPENSSL_ROOT_DIR "${_3rd_prebuilt}/openssl/msvc"
                CACHE PATH "OpenSSL prebuilt (MSVC)" FORCE)
            set(_ssl_lib    "${OPENSSL_ROOT_DIR}/lib/libssl.lib")
            set(_crypto_lib "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib")
        endif()

        add_library(OpenSSL_SSL STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL_SSL PROPERTIES
            IMPORTED_LOCATION "${_ssl_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_ROOT_DIR}/include"
        )

        add_library(OpenSSL_Crypto STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL_Crypto PROPERTIES
            IMPORTED_LOCATION "${_crypto_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_ROOT_DIR}/include"
        )

        add_library(OpenSSL::SSL    ALIAS OpenSSL_SSL)
        add_library(OpenSSL::Crypto ALIAS OpenSSL_Crypto)

    # --- Linux: use system OpenSSL via find_package ---
    else()
        find_package(OpenSSL REQUIRED)
        # Promote to GLOBAL so curl's try_compile can find the targets.
        if(TARGET OpenSSL::SSL)
            foreach(_tgt OpenSSL::SSL OpenSSL::Crypto)
                get_target_property(_real ${_tgt} ALIASED_TARGET)
                if(_real)
                    set_target_properties(${_real} PROPERTIES IMPORTED_GLOBAL TRUE)
                else()
                    set_target_properties(${_tgt} PROPERTIES IMPORTED_GLOBAL TRUE)
                endif()
            endforeach()
        endif()
    endif()

    # openssl-cmake-3 wraps OpenSSL::* as ssl / crypto INTERFACE targets.
    set(SYSTEM_OPENSSL ON CACHE BOOL "" FORCE)
    set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
    add_subdirectory(${_3rd_source}/openssl-cmake-3
                     ${CMAKE_BINARY_DIR}/3rd/openssl-cmake)
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
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)

    # Block Perl detection so curl skips add_subdirectory(docs) —
    # docs/ was deleted from vendored source to save ~5M.
    set(CMAKE_DISABLE_FIND_PACKAGE_Perl ON CACHE BOOL "" FORCE)
    add_subdirectory(${_3rd_source}/curl-8.21.0
                     ${CMAKE_BINARY_DIR}/3rd/curl)
endif()
