#include "test_common.h"

#include <httpclient/httpclient.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif

#define SEP printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

// Truncate + print a preview of the first N chars
static void printPreview(const char* label, const std::string& s, size_t limit = 200) {
    size_t len = s.size();
    if (len <= limit) {
        printf("    %s: %s\n", label, s.c_str());
    } else {
        printf("    %s (%zu chars, showing first %zu):\n    %s...\n",
               label, len, limit, s.substr(0, limit).c_str());
    }
}


// ═══════════════════════════════════════════════════════════════
//  Local HTTPS test server
//  Uses CreateProcess (Win) or fork+exec (Unix) so we can
//  terminate the process directly.  _popen / _pclose cannot kill
//  a server that runs serve_forever().
// ═══════════════════════════════════════════════════════════════

class LocalHttpsServer {
public:
    ~LocalHttpsServer() { stop(); }

    bool start() {
        const char* serverDir = HTTPS_SERVER_DIR;

        // Try candidates: probe for a working Python
        const char* candidates[] = {
#ifdef TEST_PYTHON
            TEST_PYTHON,
#endif
            "python3",
            "python",
        };
        const char* python = nullptr;
        for (const char* py : candidates) {
            if (py[0] == '\0') continue;
            // Quick probe
            std::string probeCmd = std::string(py) + " -c \"print('ok')\"";
            FILE* probe = _popen(probeCmd.c_str(), "r");
            if (!probe) continue;
            char buf[128] = {};
            bool ok = (fgets(buf, sizeof(buf), probe) != nullptr);
            _pclose(probe);
            if (ok) { python = py; break; }
        }
        if (!python) {
            printf("  (Python not available — HTTPS integration tests skipped)\n");
            return false;
        }

#ifdef _WIN32
        // Windows: CreateProcess so we have a handle to terminate later
        std::string cmdLine = std::string(python) + " \"" + serverDir + "/server.py\"";
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;

        // Create a pipe to read the "READY:<port>" line
        HANDLE hReadPipe, hWritePipe;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            printf("  CreatePipe failed\n");
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = hWritePipe;
        si.hStdError  = hWritePipe;
        si.dwFlags   |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi = {};
        BOOL created = CreateProcessA(
            nullptr, &cmdLine[0], nullptr, nullptr,
            TRUE, 0, nullptr, nullptr, &si, &pi);
        CloseHandle(hWritePipe);   // keep only read end
        if (!created) {
            printf("  CreateProcess failed (error %lu)\n", GetLastError());
            CloseHandle(hReadPipe);
            return false;
        }
        m_hProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        // Read the READY:<port> line
        char line[512] = {};
        DWORD bytesRead = 0;
        std::string accum;
        while (true) {
            char ch;
            if (!ReadFile(hReadPipe, &ch, 1, &bytesRead, nullptr) || bytesRead == 0)
                break;
            if (ch == '\n') break;
            accum += ch;
        }
        CloseHandle(hReadPipe);

        if (sscanf(accum.c_str(), "READY:%d", &m_port) == 1) {
            printf("  local HTTPS server listening on port %d (%s)\n", m_port, python);
            return true;
        }
        printf("  server output: %s\n", accum.c_str());
        stop();
        return false;

#else
        // Unix: popen + SIGTERM
        std::string cmd = std::string(python) + " \"" + serverDir + "/server.py\"";
        m_pipe = popen(cmd.c_str(), "r");
        if (!m_pipe) return false;

        char line[512] = {};
        if (!fgets(line, sizeof(line), m_pipe)) {
            stop(); return false;
        }
        if (sscanf(line, "READY:%d", &m_port) == 1) {
            printf("  local HTTPS server listening on port %d (%s)\n", m_port, python);
            return true;
        }
        printf("  server output: %s", line);
        while (fgets(line, sizeof(line), m_pipe))
            printf("  %s", line);
        stop();
        return false;
#endif
    }

    void stop() {
#ifdef _WIN32
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
#else
        if (m_pipe) {
            pclose(m_pipe);       // sends SIGPIPE / SIGHUP
            m_pipe = nullptr;
        }
#endif
    }

    int port() const { return m_port; }
    std::string baseUrl() const { return "https://localhost:" + std::to_string(m_port); }

private:
#ifdef _WIN32
    HANDLE m_hProcess = nullptr;
#else
    FILE*  m_pipe = nullptr;
#endif
    int    m_port = 0;
};


// ═══════════════════════════════════════════════════════════════
//  Phase 1 — no server needed
// ═══════════════════════════════════════════════════════════════

static void test_httpsclient_response_ok(TestRunner& t) {
    SEP;
    printf("[1/7] HttpResponse::ok() — pure struct logic, no I/O\n\n");
    HttpResponse okResp{200, "", {}, ""};
    HttpResponse errResp{404, "", {}, ""};
    HttpResponse netErr{0, "", {}, "connection refused"};

    t.check(okResp.ok(),   "200 → ok() == true");
    t.check(!errResp.ok(), "404 → ok() == false");
    t.check(!netErr.ok(),  "network error → ok() == false");
}

static void test_httpsclient_invalid_url(TestRunner& t) {
    SEP;
    printf("[2/7] GET invalid URL — expect failure\n\n");

    printf("    REQUEST : GET https://invalid.domain.does.not.exist.example/nope\n");
    auto res = HttpsClient::get("https://invalid.domain.does.not.exist.example/nope");
    printf("    RESULT  : ok=%s  error=%s\n",
           res.ok() ? "true" : "false", res.error.c_str());

    t.check(!res.ok(),          "invalid URL → ok() == false");
    t.check(!res.error.empty(), "error message is not empty");
}

static void test_httpsclient_config(TestRunner& t) {
    SEP;
    printf("[3/7] global config — setters and restore\n\n");

    HttpsClient::setTimeout(60);
    HttpsClient::setConnectTimeout(15);
    HttpsClient::setVerifySsl(true);
    HttpsClient::setVerifySsl(false);
    HttpsClient::setVerifySsl(true);
    HttpsClient::setCaBundlePath("cacert.pem");
    HttpsClient::setCaBundlePath("cacert.pem");

    t.check(true, "global config setters called without crash");

    // Restore defaults for subsequent tests
    HttpsClient::setCaBundlePath("");
    HttpsClient::setVerifySsl(false);
}


// ═══════════════════════════════════════════════════════════════
//  Phase 2 — local HTTPS echo server
// ═══════════════════════════════════════════════════════════════

static void test_httpsclient_get(TestRunner& t, const std::string& baseUrl) {
    SEP;
    printf("[4/7] GET /get — basic request\n\n");

    std::string url = baseUrl + "/get";
    printf("    REQUEST : GET %s\n", url.c_str());

    auto res = HttpsClient::get(url);

    printf("    RESPONSE: HTTP %ld  %zu bytes\n", res.statusCode, res.body.size());
    printPreview("body", res.body);
    printf("    headers : %zu entries\n", res.headers.size());
    for (auto& [k, v] : res.headers)
        printf("      %s: %s\n", k.c_str(), v.c_str());

    t.check(res.ok(), "HTTP 200");
    t.check(res.body.find("\"method\"") != std::string::npos, "body contains method");
    t.check(res.body.find("\"GET\"")    != std::string::npos, "method is GET");
}

static void test_httpsclient_get_with_headers(TestRunner& t, const std::string& baseUrl) {
    SEP;
    printf("[5/7] GET /get — with custom headers\n\n");

    std::string url = baseUrl + "/get";
    printf("    REQUEST : GET %s\n", url.c_str());
    printf("    headers : X-Custom-Header: test-value, Accept: application/json\n");

    auto res = HttpsClient::get(url,
                                {{"X-Custom-Header", "test-value"},
                                 {"Accept",         "application/json"}});

    printf("    RESPONSE: HTTP %ld  %zu bytes\n", res.statusCode, res.body.size());
    printPreview("body", res.body);

    t.check(res.ok(), "HTTP 200");
    t.check(res.body.find("X-Custom-Header") != std::string::npos ||
            res.body.find("x-custom-header") != std::string::npos,
            "custom header echoed");
}

static void test_httpsclient_post_json(TestRunner& t, const std::string& baseUrl) {
    SEP;
    printf("[6/7] POST /post — JSON body\n\n");

    std::string url  = baseUrl + "/post";
    std::string body = R"({"hello":"world","count":42})";
    printf("    REQUEST : POST %s\n", url.c_str());
    printf("    body    : %s\n", body.c_str());

    auto res = HttpsClient::postJson(url, body);

    printf("    RESPONSE: HTTP %ld  %zu bytes\n", res.statusCode, res.body.size());
    printPreview("body", res.body);

    t.check(res.ok(), "HTTP 200");
    t.check(res.body.find("hello") != std::string::npos, "body echoed hello");
    t.check(res.body.find("world") != std::string::npos, "body echoed world");
    t.check(res.body.find("count") != std::string::npos, "body echoed count");
}

static void test_httpsclient_post_form(TestRunner& t, const std::string& baseUrl) {
    SEP;
    printf("[7/7] POST /post — form-urlencoded\n\n");

    std::string url = baseUrl + "/post";
    printf("    REQUEST : POST %s\n", url.c_str());
    printf("    body    : user=admin&action=login\n");

    auto res = HttpsClient::postForm(url,
                                     {{"user", "admin"}, {"action", "login"}});

    printf("    RESPONSE: HTTP %ld  %zu bytes\n", res.statusCode, res.body.size());
    printPreview("body", res.body);

    t.check(res.ok(), "HTTP 200");
    t.check(res.body.find("user")   != std::string::npos, "body echoed user");
    t.check(res.body.find("admin")  != std::string::npos, "body echoed admin");
    t.check(res.body.find("action") != std::string::npos, "body echoed action");
    t.check(res.body.find("login")  != std::string::npos, "body echoed login");
}


// ═══════════════════════════════════════════════════════════════
//  main
// ═══════════════════════════════════════════════════════════════

int main() {
    printf("\n"
           "╔══════════════════════════════════════════╗\n"
           "║       HttpsClient  Test Suite            ║\n"
           "╚══════════════════════════════════════════╝\n");

    TestRunner t1;
    test_httpsclient_response_ok(t1);
    test_httpsclient_invalid_url(t1);
    test_httpsclient_config(t1);

    SEP;
    printf("[Phase 2] starting local HTTPS echo server...\n");
    LocalHttpsServer server;
    if (!server.start()) {
        printf("  (integration tests skipped)\n");
        SEP;
        return t1.finish();
    }

    TestRunner t2;
    test_httpsclient_get(t2,             server.baseUrl());
    test_httpsclient_get_with_headers(t2, server.baseUrl());
    test_httpsclient_post_json(t2,       server.baseUrl());
    test_httpsclient_post_form(t2,       server.baseUrl());

    server.stop();
    printf("  server stopped.\n");

    SEP;
    printf("\n  Total: %d assertions, %d failures\n",
           t1.asserts + t2.asserts, t1.failures + t2.failures);
    SEP;
    return (t1.failures + t2.failures) > 0 ? 1 : 0;
}
