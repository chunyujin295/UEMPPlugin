#include <logger/logger.h>
#include <yamltool/yamlnode.h>
#include <yamltool/yamltool.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

namespace fs = std::filesystem;
using YT = YamlTool::YamlTool;
using YamlNode = YamlTool::YamlNode;

// ---- test helpers ----
static int  g_asserts = 0;
static int  g_failures = 0;
static void check(bool cond, const char* msg) {
    g_asserts++;
    if (!cond) { g_failures++; printf("  FAIL: %s\n", msg); }
    else       { printf("  PASS: %s\n", msg); }
}

// ---- Logger tests ----
void test_logger_levels() {
    printf("\n[Logger] log levels...\n");
    LOG_TRACE("trace message:", 1);
    LOG_DEBUG("debug message:", 2.5);
    LOG_INFO("info message:", "hello");
    LOG_WARN("warn message:", true);
    LOG_ERROR("error message:", 3.14);
    LOG_CRITI("critical message:", std::string("world"));
    printf("  (check console for log output above)\n");
    check(true, "all log levels called without crash");
}

void test_logger_callback() {
    printf("\n[Logger] callback sink...\n");
    int callCount = 0;
    std::string lastMsg;

    std::string sinkId = Logger::addCallBack(
        [&](const LogMsg& m) {
            callCount++;
            lastMsg = m.msg;
        },
        LogLevel::Info);

    LOG_INFO("callback test message");
    Logger::shutdown();

    check(callCount > 0, "callback was invoked");
    check(lastMsg.find("callback test message") != std::string::npos,
          "callback received expected message");

    Logger::removeCallBack(sinkId);
}

void test_logger_set_config_path() {
    printf("\n[Logger] setConfigPath...\n");
    fs::path tmpDir = fs::temp_directory_path() / "uemp_logger_test";
    fs::create_directories(tmpDir);
    std::string cfgPath = (tmpDir / "log_config.yaml").string();

    Logger::setConfigPath(cfgPath, false);
    LOG_INFO("after setConfigPath");
    Logger::shutdown();

    check(fs::exists(cfgPath), "config file was created");
    fs::remove_all(tmpDir);
}

// ---- YamlTool tests ----
void test_yamltool_basic() {
    printf("\n[YamlTool] load / save / get / set...\n");
    fs::path tmpDir = fs::temp_directory_path() / "uemp_yaml_test";
    fs::create_directories(tmpDir);
    std::string yamlPath = (tmpDir / "test.yaml").string();

    YamlNode root;
    YT::set<int>(root, "answer", 42);
    YT::set<std::string>(root, "name", "uemp");
    YT::set<double>(root, "pi", 3.14159);
    YT::set<bool>(root, "enabled", true);
    YT::saveAsFile(root, yamlPath);
    check(fs::exists(yamlPath), "YAML file saved");

    YamlNode loaded;
    bool ok = YT::loadFile(loaded, yamlPath);
    check(ok, "YAML file loaded");

    int         answer  = YT::getDef<int>(loaded, "answer", 0);
    std::string name    = YT::getDef<std::string>(loaded, "name", "");
    double      pi      = YT::getDef<double>(loaded, "pi", 0.0);
    bool        enabled = YT::getDef<bool>(loaded, "enabled", false);

    check(answer == 42,              "int round-trip");
    check(name == "uemp",            "string round-trip");
    check(pi > 3.14 && pi < 3.142,  "double round-trip");
    check(enabled == true,           "bool round-trip");

    int missing = YT::getDef<int>(loaded, "no_such_key", 999);
    check(missing == 999, "getDef returns default for missing key");

    YamlNode nullNode;
    check(nullNode.isNull(), "default-constructed node is null");
    int nullDef = YT::getDef<int>(nullNode, "anything", -1);
    check(nullDef == -1, "getDef on null node returns default safely");

    bool wrote = YT::setDef<int>(loaded, "new_key", 100);
    check(wrote, "setDef returns true when key was absent");
    int newVal = YT::getDef<int>(loaded, "new_key", 0);
    check(newVal == 100, "setDef actually wrote the value");

    bool wroteAgain = YT::setDef<int>(loaded, "answer", 99);
    check(!wroteAgain, "setDef returns false when key already exists");
    int answer2 = YT::getDef<int>(loaded, "answer", 0);
    check(answer2 == 42, "setDef did not overwrite existing value");

    fs::remove_all(tmpDir);
}

void test_yamltool_node_types() {
    printf("\n[YamlTool] node types...\n");

    YamlNode mapNode;
    YT::set<int>(mapNode, "a", 1);
    check(mapNode.isMap(),   "node with keys is Map");
    check(!mapNode.isNull(), "non-empty node is not null");

    fs::path tmpDir = fs::temp_directory_path() / "uemp_yaml_seq_test";
    fs::create_directories(tmpDir);
    std::string seqPath = (tmpDir / "seq.yaml").string();

    YamlNode root;
    YamlNode item1;
    YT::set<int>(item1, "id", 1);
    YamlNode item2;
    YT::set<int>(item2, "id", 2);
    YT::pushBack(root, item1);
    YT::pushBack(root, item2);
    YT::saveAsFile(root, seqPath);

    YamlNode loadedSeq;
    YT::loadFile(loadedSeq, seqPath);
    check(loadedSeq.isSequence(), "loaded sequence is Sequence");
    check(loadedSeq.size() == 2,  "sequence has 2 elements");

    YamlNode first = YT::getSequenceNode(loadedSeq, 0);
    int firstId = YT::getDef<int>(first, "id", -1);
    check(firstId == 1, "first sequence element id == 1");

    fs::remove_all(tmpDir);
}

void test_yamltool_set_null() {
    printf("\n[YamlTool] setNull / setNullDef...\n");

    YamlNode node;

    bool wrote = YT::setNullDef(node, "optional_field");
    check(wrote, "setNullDef writes when key missing");

    bool wroteAgain = YT::setNullDef(node, "optional_field");
    check(!wroteAgain, "setNullDef skips when key exists");

    YT::setNull(node, "optional_field");
    check(true, "setNull does not crash");
}

// ---- OpenSSL tests ----
void test_openssl_version() {
    printf("\n[OpenSSL] version...\n");
    long ver = OpenSSL_version_num();
    printf("  OpenSSL version: 0x%lx\n", (unsigned long)ver);
    check(ver >= 0x30500000, "OpenSSL >= 3.5.0");    // 3.5.7 → 0x30507000
}

void test_openssl_sha256() {
    printf("\n[OpenSSL] SHA256...\n");
    const char* input = "UEMPPlugin test";
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    check(ctx != nullptr, "EVP_MD_CTX_new");

    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    check(ok == 1, "EVP_DigestInit_ex");

    ok = EVP_DigestUpdate(ctx, input, strlen(input));
    check(ok == 1, "EVP_DigestUpdate");

    ok = EVP_DigestFinal_ex(ctx, digest, &digestLen);
    check(ok == 1, "EVP_DigestFinal_ex");
    check(digestLen == 32, "SHA256 digest is 32 bytes");

    // Verify determinism: same input → same hash
    unsigned char digest2[EVP_MAX_MD_SIZE];
    unsigned int digestLen2 = 0;
    EVP_MD_CTX* ctx2 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx2, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx2, input, strlen(input));
    EVP_DigestFinal_ex(ctx2, digest2, &digestLen2);
    check(memcmp(digest, digest2, 32) == 0, "SHA256 is deterministic");

    EVP_MD_CTX_free(ctx);
    EVP_MD_CTX_free(ctx2);
}

void test_openssl_random() {
    printf("\n[OpenSSL] random bytes...\n");
    unsigned char buf1[32];
    unsigned char buf2[32];

    int rc = RAND_bytes(buf1, sizeof(buf1));
    check(rc == 1, "RAND_bytes first call");

    rc = RAND_bytes(buf2, sizeof(buf2));
    check(rc == 1, "RAND_bytes second call");

    check(memcmp(buf1, buf2, 32) != 0, "two random buffers differ");
}

// ---- libcurl tests ----
static size_t write_callback(void* data, size_t size, size_t nmemb, void* userp) {
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

void test_curl_version() {
    printf("\n[libcurl] version info...\n");
    curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    check(info != nullptr, "curl_version_info returns data");
    check(info->age >= 10, "version info age >= 10");

    printf("  libcurl version: %s\n", info->version);

    // Verify SSL backend is registered
    bool hasSsl = false;
    for (const char* const* p = info->protocols; *p; ++p) {
        if (strcmp(*p, "https") == 0) { hasSsl = true; break; }
    }
    check(hasSsl, "HTTPS protocol supported");
}

void test_curl_init_cleanup() {
    printf("\n[libcurl] init / cleanup...\n");
    CURL* curl = curl_easy_init();
    check(curl != nullptr, "curl_easy_init");

    curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

    curl_easy_cleanup(curl);
    check(true, "curl_easy_cleanup without crash");
}

void test_curl_https_get() {
    printf("\n[libcurl] HTTPS GET...\n");
    CURL* curl = curl_easy_init();
    check(curl != nullptr, "curl_easy_init");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        check(httpCode == 200, "HTTP 200");
        check(response.find("httpbin.org") != std::string::npos, "response contains host");
    } else {
        printf("  (network unavailable: %s)\n", curl_easy_strerror(res));
        check(true, "skipped — no network");
    }

    curl_easy_cleanup(curl);
}

// ---- main ----
int main() {
    printf("=== UEMPPlugin Common Library Tests ===\n");

    // YamlTool
    test_yamltool_basic();
    test_yamltool_node_types();
    test_yamltool_set_null();

    // Logger (depends on yamltool internally for config parsing)
    test_logger_levels();
    test_logger_set_config_path();
    test_logger_callback();

    // OpenSSL
    test_openssl_version();
    test_openssl_sha256();
    test_openssl_random();

    // libcurl
    test_curl_version();
    test_curl_init_cleanup();
    test_curl_https_get();

    printf("\n=== Results: %d assertions, %d failures ===\n", g_asserts, g_failures);
    return g_failures > 0 ? 1 : 0;
}
