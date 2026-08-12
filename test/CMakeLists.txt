# =============================================================================
# Test targets — one executable per module, registered as CTest tests.
# =============================================================================

# ---- Logger ----
add_executable(test_logger    test_logger.cpp)
add_executable(test_yamltool  test_yamltool.cpp)
add_executable(test_openssl   test_openssl.cpp)
add_executable(test_curl      test_curl.cpp)
add_executable(test_httpclient test_httpclient.cpp)
add_executable(test_json      test_json.cpp)

# ---- Dependencies (minimal per target) ----
target_link_libraries(test_logger    PRIVATE common)
target_link_libraries(test_yamltool  PRIVATE common)
target_link_libraries(test_openssl   PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(test_curl      PRIVATE libcurl)
target_link_libraries(test_httpclient PRIVATE common OpenSSL::SSL OpenSSL::Crypto libcurl)
target_link_libraries(test_json      PRIVATE nlohmann_json)

# Pass absolute path to the HTTPS test server dir (so _popen can find server.py)
# regardless of the working directory at runtime.
get_filename_component(_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Find Python at configure time — helps diagnostics, but runtime still
# does its own PATH probe (probePython) as fallback.
find_program(TEST_PYTHON
    NAMES python3 python)
if(TEST_PYTHON)
    message(STATUS "Test Python: ${TEST_PYTHON}")
    target_compile_definitions(test_httpclient PRIVATE
        TEST_PYTHON="${TEST_PYTHON}")
else()
    message(STATUS "Test Python not found — HTTPS integration tests will skip at runtime")
endif()

target_compile_definitions(test_httpclient PRIVATE
    HTTPS_SERVER_DIR="${_repo_root}/test/https_server")

# ---- Copy cacert.pem & https_server to output dir ----
# (needed by test_curl / test_httpclient)
add_custom_command(TARGET test_curl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_repo_root}/3rd/cacert.pem"
        "${CMAKE_BINARY_DIR}/out/cacert.pem")

add_custom_command(TARGET test_httpclient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_repo_root}/3rd/cacert.pem"
        "${CMAKE_BINARY_DIR}/out/cacert.pem"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${_repo_root}/test/https_server"
        "${CMAKE_BINARY_DIR}/out/https_server")

# ---- Register CTest tests ----
add_test(NAME logger     COMMAND test_logger)
add_test(NAME yamltool   COMMAND test_yamltool)
add_test(NAME openssl    COMMAND test_openssl)
add_test(NAME curl       COMMAND test_curl)
add_test(NAME httpclient COMMAND test_httpclient)
add_test(NAME json       COMMAND test_json)
