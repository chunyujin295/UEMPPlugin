cmake_minimum_required(VERSION 3.21)
project(UEMPPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# ---- Output directories ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)

# ---- Third-party dependencies ----
include(cmake/3rd.cmake)

# ---- Common library ----
add_subdirectory(common)

# ---- Tests ----
option(BUILD_TEST "Build test programs" ON)
if (BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
