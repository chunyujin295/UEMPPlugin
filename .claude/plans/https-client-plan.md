# HTTPS Client 封装计划

## 1. 背景分析

项目已有基础设施：
- `common` 库已链接 **libcurl 8.21.0** + **OpenSSL 3.5.7**
- 测试代码中已有原始 curl 调用（`test/main.cpp` 的 `test_curl_https_get` 等）
- 项目采用 **PIMPL 模式**（如 Logger → LogPrivate）
- 目录约定：`include/common/<模块>/` → `private/<模块>/` → `src/<模块>/`
- 已有 `common/CMakeLists.txt` 使用 `GLOB_RECURSE` 自动收集源文件 —— 新增 `.cpp` / `.h` 即可生效

## 2. 新增文件

```
common/
├── include/common/httpsclient/
│   ├── export.h                 # HTTPS_CLIENT_API 导出宏
│   └── httpsclient.h            # 公开头文件（HttpsClient 类 + HttpResponse 结构体）
├── private/httpsclient/
│   ├── httpsclient_p.h          # PIMPL 私有实现声明
│   └── httpsclient_p.cpp        # PIMPL 私有实现（curl 调用细节）
└── src/httpsclient/
    └── httpsclient.cpp           # 公开 API 薄封装（转发到 Private）
doc/
└── https_client.md               # HTTPS 技术原理 + 使用文档
```

> **不需要修改** `common/CMakeLists.txt`，因为它使用 `file(GLOB_RECURSE)` 自动收集 `src/`、`private/`、`include/` 下的源文件。

## 3. API 设计

采用 **RAII 实例化风格**（与 Logger 的全静态风格互补 —— 因为 HTTP 客户端需要不同的超时、URL 等实例配置）。

### 3.1 HttpResponse 结构体

```cpp
struct HttpResponse {
    long        statusCode;   // HTTP 状态码（200, 404, 500 ...），连接失败时为 0
    std::string body;         // 响应体
    std::string error;        // 错误信息（成功时为空）
    bool        ok() const;   // statusCode >= 200 && statusCode < 300
};
```

### 3.2 HttpsClient 类

```cpp
class HttpsClient {
public:
    HttpsClient();
    ~HttpsClient();
    // 禁止拷贝，允许移动
    HttpsClient(const HttpsClient&) = delete;
    HttpsClient& operator=(const HttpsClient&) = delete;
    HttpsClient(HttpsClient&&) noexcept;
    HttpsClient& operator=(HttpsClient&&) noexcept;

    // ── 配置（可选，有合理默认值）──
    void setTimeout(long seconds);         // 总超时，默认 30s
    void setConnectTimeout(long seconds);  // 连接超时，默认 10s
    void setVerifySsl(bool verify);        // 是否验证 SSL 证书，默认 true
    void setCaCertPath(const std::string& path); // 自定义 CA 证书路径

    // ── 请求 ──
    HttpResponse get(const std::string& url);
    HttpResponse post(const std::string& url,
                      const std::string& body,
                      const std::string& contentType = "application/json");

    // ── 便捷静态方法（单次请求，用默认配置）──
    static HttpResponse quickGet(const std::string& url);
    static HttpResponse quickPost(const std::string& url, const std::string& body);
};
```

### 3.3 设计理由

| 决策 | 理由 |
|------|------|
| RAII 实例化 | 允许不同实例有不同超时/证书配置，`curl_easy_init/cleanup` 与构造/析构自然绑定 |
| 禁止拷贝允许移动 | curl handle 不可共享，移动语义释放旧 handle |
| `quickGet/quickPost` 静态方法 | 覆盖"我就发一次请求、不想创建对象"的简单场景 |
| `setVerifySsl(false)` | 开发/内网自签证书环境必备开关 |
| 错误信息在 `HttpResponse.error` 中 | 不用异常，与项目现有的 `check()` 风格一致 |

## 4. 实现要点

- **底层**：每个 `HttpsClient` 实例持有一个 `CURL*` handle
- **write callback**：写入 `std::string` 响应体
- **SSL**：默认 `CURLOPT_SSL_VERIFYPEER` 开启，走系统 CA 证书；可用 `setVerifySsl(false)` 关闭（内网环境）
- **错误处理**：`curl_easy_perform` 返回非 `CURLE_OK` 时，错误信息写入 `HttpResponse::error`
- **线程安全**：每个实例独立的 `CURL*`，不共享全局状态，天然线程安全

## 5. 文档计划（`doc/https_client.md`）

| 章节 | 内容 |
|------|------|
| HTTPS 技术原理 | 对称加密 vs 非对称加密、TLS 握手流程、证书链验证、CA 体系 |
| 封装设计 | PIMPL 模式说明、为什么选 RAII 实例化、与 Logger 风格的对比 |
| 使用示例 | GET/POST 代码示例、自定义超时、跳过证书验证、静态便捷方法 |
| 前置条件 | 系统 CA 证书、运行库依赖（OpenSSL DLL）、内网自签证书处理 |
| 常见问题 | 证书验证失败、超时设置、POST body 格式 |

## 6. 测试

在 `test/main.cpp` 中追加 `[HttpsClient]` 测试区块，覆盖：
1. 默认构造 / 析构
2. GET 请求（网络不可用时 skip）
3. POST 请求（网络不可用时 skip）
4. 超时设置
5. SSL 验证开关
6. 移动语义
