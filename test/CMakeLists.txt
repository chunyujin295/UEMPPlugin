add_executable(common_test main.cpp)

target_link_libraries(common_test PRIVATE
    common
    OpenSSL::SSL
    OpenSSL::Crypto
    libcurl
)

add_test(NAME common_test COMMAND common_test)
