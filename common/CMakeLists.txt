# =============================================================================
# Common Library — unified logger + yamltool
# =============================================================================

option(COMMON_BUILD_SHARED "Build common as shared library" ON)

# ---- Library type ----
if (COMMON_BUILD_SHARED)
    set(common_lib_type SHARED)
    set(CMAKE_DEBUG_POSTFIX d)
else()
    set(common_lib_type STATIC)
endif()

add_library(common ${common_lib_type} "")

# ---- Export macros ----
# Both LOGGER_API and YAML_TOOL_API need their BUILDING_LIBRARY define
if (COMMON_BUILD_SHARED)
    target_compile_definitions(common
        PRIVATE
            LOGGER_BUILDING_LIBRARY
            YAML_TOOL_BUILDING_LIBRARY
            HTTP_CLIENT_BUILDING_LIBRARY
    )
    set_target_properties(common PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
endif()

# ---- Sources ----
file(GLOB_RECURSE common_srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE common_private CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/private/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/private/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/private/*.hpp"
)
file(GLOB_RECURSE common_public CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

target_sources(common
    PRIVATE
        ${common_srcs}
        ${common_private}
        ${common_public}
)

# ---- Include directories ----
target_include_directories(common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/common>
        $<INSTALL_INTERFACE:include/common>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
        ${CMAKE_CURRENT_SOURCE_DIR}/private/logger
        ${CMAKE_CURRENT_SOURCE_DIR}/private/yamltool
        ${CMAKE_CURRENT_SOURCE_DIR}/private/httpclient
)

# ---- Dependencies (set up by cmake/3rd.cmake) ----
target_link_libraries(common
    PRIVATE
        spdlog
        yaml-cpp
        OpenSSL::SSL
        OpenSSL::Crypto
        libcurl
)

# ---- Output directories ----
set_target_properties(common PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out
)

# ---- Install (optional) ----
option(COMMON_INSTALL "Install common library" OFF)

if (COMMON_INSTALL)
    include(GNUInstallDirs)

    install(TARGETS common
        EXPORT commonTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY include/common
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT commonTargets
        FILE commonTargets.cmake
        NAMESPACE common::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/common
    )
endif()
