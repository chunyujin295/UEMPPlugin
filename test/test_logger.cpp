#include "test_common.h"

#include <logger/logger.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ---- Logger tests ----
static void test_logger_levels(TestRunner& t) {
    printf("\n[Logger] log levels...\n");
    LOG_TRACE("trace message:", 1);
    LOG_DEBUG("debug message:", 2.5);
    LOG_INFO("info message:", "hello");
    LOG_WARN("warn message:", true);
    LOG_ERROR("error message:", 3.14);
    LOG_CRITI("critical message:", std::string("world"));
    printf("  (check console for log output above)\n");
    t.check(true, "all log levels called without crash");
}

static void test_logger_callback(TestRunner& t) {
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

    t.check(callCount > 0, "callback was invoked");
    t.check(lastMsg.find("callback test message") != std::string::npos,
            "callback received expected message");

    Logger::removeCallBack(sinkId);
}

static void test_logger_set_config_path(TestRunner& t) {
    printf("\n[Logger] setConfigPath...\n");
    fs::path tmpDir = fs::temp_directory_path() / "uemp_logger_test";
    fs::create_directories(tmpDir);
    std::string cfgPath = (tmpDir / "log_config.yaml").string();

    Logger::setConfigPath(cfgPath, false);
    LOG_INFO("after setConfigPath");
    Logger::shutdown();

    t.check(fs::exists(cfgPath), "config file was created");
    fs::remove_all(tmpDir);
}

int main() {
    printf("=== Logger Tests ===\n");
    TestRunner t;
    test_logger_levels(t);
    test_logger_set_config_path(t);
    test_logger_callback(t);
    return t.finish();
}
