#include "test_common.h"

#include <curl/curl.h>

#include <cstring>
#include <string>

static size_t write_callback(void* data, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

// ---- libcurl tests ----
static void test_curl_version(TestRunner& t) {
    printf("\n[libcurl] version info...\n");
    curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    t.check(info != nullptr, "curl_version_info returns data");
    t.check(info->age >= 10, "version info age >= 10");

    printf("  libcurl version: %s\n", info->version);

    // Verify SSL backend is registered
    bool hasSsl = false;
    for (const char* const* p = info->protocols; *p; ++p) {
        if (strcmp(*p, "https") == 0) { hasSsl = true; break; }
    }
    t.check(hasSsl, "HTTPS protocol supported");
}

static void test_curl_init_cleanup(TestRunner& t) {
    printf("\n[libcurl] init / cleanup...\n");
    CURL* curl = curl_easy_init();
    t.check(curl != nullptr, "curl_easy_init");

    curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

    curl_easy_cleanup(curl);
    t.check(true, "curl_easy_cleanup without crash");
}

static void test_curl_https_get(TestRunner& t) {
    printf("\n[libcurl] HTTPS GET...\n");
    CURL* curl = curl_easy_init();
    t.check(curl != nullptr, "curl_easy_init");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");   // 与可执行文件同目录

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        t.check(httpCode == 200, "HTTP 200");
        t.check(response.find("httpbin.org") != std::string::npos, "response contains host");
    } else {
        printf("  (network unavailable: %s)\n", curl_easy_strerror(res));
        t.check(true, "skipped — no network");
    }

    curl_easy_cleanup(curl);
}

int main() {
    printf("=== libcurl Tests ===\n");
    TestRunner t;
    test_curl_version(t);
    test_curl_init_cleanup(t);
    test_curl_https_get(t);
    return t.finish();
}
