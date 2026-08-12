# HTTPS 客户端开发文档

本文档从零开始讲解 HTTPS 的技术原理，并详细说明 `HttpsClient` 的封装设计、API 用法以及使用前提条件。

---

## 目录

1. [HTTPS 技术原理](#1-https-技术原理)
   - [1.1 为什么需要 HTTPS](#11-为什么需要-https)
   - [1.2 TLS 握手全过程](#12-tls-握手全过程)
   - [1.3 证书与证书链](#13-证书与证书链)
   - [1.4 对称加密与非对称加密](#14-对称加密与非对称加密)
   - [1.5 前向安全性](#15-前向安全性)
2. [封装设计说明](#2-封装设计说明)
   - [2.1 为什么选择 libcurl](#21-为什么选择-libcurl)
   - [2.2 架构与模式](#22-架构与模式)
   - [2.3 线程安全设计](#23-线程安全设计)
3. [API 参考](#3-api-参考)
4. [使用前提条件](#4-使用前提条件)
   - [4.1 OpenSSL 运行时 DLL](#41-openssl-运行时-dll)
   - [4.2 CA 根证书](#42-ca-根证书)
   - [4.3 服务端自签证书](#43-服务端自签证书)
   - [4.4 客户端证书（mTLS）](#44-客户端证书mtls)
   - [4.5 Token / API Key](#45-token--api-key)
5. [常见问题与排查](#5-常见问题与排查)

---

## 1. HTTPS 技术原理

### 1.1 为什么需要 HTTPS

HTTP（HyperText Transfer Protocol）是明文协议。这意味着：

- **任何中间节点**（路由器、交换机、ISP）都能看到完整的请求和响应内容
- **攻击者可以篡改**数据包，插入广告或恶意代码（中间人攻击，MITM）
- **无法验证**通信对方是否真的是目标服务器

HTTPS = HTTP + TLS（Transport Layer Security），它在 HTTP 和 TCP 之间插入了一层加密层，实现了三个核心目标：

| 目标 | 手段 | 说明 |
|------|------|------|
| **机密性** | 对称加密 | 通信内容只有双方能解密，中间节点看到的是乱码 |
| **完整性** | MAC（消息认证码） | 任何篡改都会被检测到 |
| **身份验证** | 证书 + 非对称加密 | 确认你连接的就是目标服务器，而非冒牌货 |

### 1.2 TLS 握手全过程

当你调用 `HttpsClient::get("https://api.example.com/data")` 时，底层发生了以下步骤：

```
客户端 (libcurl + OpenSSL)                    服务器 (api.example.com)
    │                                                  │
    │  ──── (1) ClientHello ────────────────────────▶  │
    │        支持的 TLS 版本 (1.2, 1.3)                │
    │        支持的密码套件列表                          │
    │        客户端随机数 (Client Random)               │
    │                                                  │
    │  ◀──── (2) ServerHello ────────────────────────  │
    │        选定的 TLS 版本                            │
    │        选定的密码套件                              │
    │        服务器随机数 (Server Random)               │
    │                                                  │
    │  ◀──── (3) Certificate ────────────────────────  │
    │        服务器证书链                                │
    │        (api.example.com → 中间CA → 根CA)          │
    │                                                  │
    │  ◀──── (4) ServerHelloDone ────────────────────  │
    │                                                  │
    │  ──── (客户端验证证书) ────                        │
    │    • 证书链是否受信任（系统/CA bundle 的根证书）    │
    │    • 证书是否过期                                  │
    │    • 证书的 CN/SAN 是否匹配域名                     │
    │                                                  │
    │  ──── (5) ClientKeyExchange ──────────────────▶  │
    │        用服务器公钥加密的 Pre-Master Secret        │
    │        (TLS 1.3 中这一步合并到了 ClientHello)      │
    │                                                  │
    │  ──── (6) ChangeCipherSpec ───────────────────▶  │
    │        "接下来我会用协商好的密钥加密"               │
    │                                                  │
    │  ──── (7) Finished (加密) ────────────────────▶  │
    │    ◀──── Finished (加密) ──────────────────────   │
    │                                                  │
    │  ╔══════════════════════════════════════════╗    │
    │  ║   此后所有数据用对称密钥加密传输            ║    │
    │  ╚══════════════════════════════════════════╝    │
    │                                                  │
    │  ──── (8) HTTP Request (加密) ──────────────▶  │
    │    ◀──── HTTP Response (加密) ────────────────  │
```

**TLS 1.3 的改进**：握手从 2-RTT 降低到 1-RTT，移除了不安全的密码套件，安全性更高。

### 1.3 证书与证书链

**证书**是一个由 CA（Certificate Authority，证书颁发机构）签名的文件，包含：

- 域名（如 `api.example.com`）
- 公钥
- 有效期
- 颁发者签名

**证书链**是一个信任链条：

```
根 CA 证书（自签名，预置在操作系统/CA bundle中）
  └─ 签署 ▶ 中间 CA 证书
           └─ 签署 ▶ 服务器证书（api.example.com）
```

**验证过程**：
1. 检查服务器证书是否由中间 CA 签署 → 用中间 CA 的公钥验证签名
2. 检查中间 CA 证书是否由根 CA 签署 → 用根 CA 的公钥验证签名
3. 检查根 CA 是否在受信任的根证书列表中

这就是为什么 `HttpsClient` 需要访问 **CA 根证书**：libcurl + OpenSSL 需要根证书来验证服务器证书链。

### 1.4 对称加密与非对称加密

HTTPS 混合使用两种加密方式：

| 类型 | 对称加密 | 非对称加密（公钥加密） |
|------|---------|----------------------|
| **密钥数量** | 1 个（通信双方共享） | 2 个（公钥 + 私钥） |
| **速度** | 快（Kbps～Gbps 级） | 慢（比对称慢 100～1000 倍） |
| **用途** | 加密实际数据传输 | 握手阶段安全交换对称密钥 |
| **算法示例** | AES-256-GCM, ChaCha20 | RSA-2048, ECDHE |

**为什么不用非对称加密传输所有数据？** 太慢。TLS 的设计哲学是：用非对称加密安全地交换一个临时的对称密钥，之后全部用对称加密。

### 1.5 前向安全性

**前向安全（Forward Secrecy）** 意味着：即使服务器的私钥未来被泄露，过去截获的加密通信也无法被解密。

**原理**：TLS 1.3 强制使用 ECDHE（椭圆曲线 Diffie-Hellman 临时密钥交换），每次连接生成一个临时的、用完即丢弃的密钥对。即使攻击者拿到了服务器的长期私钥，也无法还原会话密钥。

---

## 2. 封装设计说明

### 2.1 为什么选择 libcurl

| 候选方案 | 优点 | 缺点 |
|---------|------|------|
| **libcurl** ✅ | 跨平台、HTTPS 开箱即用、成熟稳定 (1998～) | C API，需封装 |
| WinHTTP | Windows 原生，无需额外 DLL | 仅 Windows，不支持 Linux |
| cpp-httplib | 纯 C++ 单头文件，使用简单 | 功能相对有限，生产环境验证不足 |
| Boost.Beast | C++ 原生异步，高性能 | 依赖 Boost 生态，引入复杂度高 |

选择 libcurl 的原因：
- **项目已集成**：`3rd/source/curl-8.21.0`，无需新增依赖
- **全平台**：Windows MSVC / Linux 均可编译
- **协议完整**：HTTP/2、重定向跟随、连接复用、代理支持
- **安全可靠**：全球数十亿设备使用，安全漏洞响应及时

### 2.2 架构与模式

本封装遵循项目已有的 **PIMPL（Pointer to Implementation）** 模式，与 `Logger`、`YamlTool` 保持一致：

```
┌─────────────────────────────────────────────────────┐
│  调用方（test/main.cpp，业务代码）                    │
│  HttpsClient::get("https://api.example.com/data")   │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│  common/include/common/httpclient/httpclient.h      │  ← 公共头文件
│  class HttpsClient {                                 │    全部静态方法
│    static HttpResponse get(...);                     │    HTTP_CLIENT_API 导出
│    static HttpResponse postJson(...);                │
│  };                                                  │
└──────────────────────┬──────────────────────────────┘
                       │  转发调用
┌──────────────────────▼──────────────────────────────┐
│  common/src/httpclient/httpclient.cpp               │  ← 薄层转发
│  HttpResponse HttpsClient::get(...) {                │    PIMPL 桥梁
│    return HttpsClientPrivate::get(...);              │
│  }                                                   │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│  common/private/httpclient/httpclient_p.cpp         │  ← 私有实现
│  class HttpsClientPrivate {                          │    真正的业务逻辑
│    static HttpResponse get(...) {                     │    libcurl 调用
│      CURL* curl = curl_easy_init();                  │    SSL 配置
│      curl_easy_setopt(curl, CURLOPT_URL, ...);       │    错误处理
│      CURLcode res = curl_easy_perform(curl);         │    日志输出
│      curl_easy_cleanup(curl);                        │
│    }                                                 │
│  };                                                  │
└──────────────────────┬──────────────────────────────┘
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
    ┌──────────┐            ┌──────────────┐
    │ libcurl  │            │   OpenSSL    │
    │ HTTP/2   │──SSL────▶  │  TLS 1.2/1.3 │
    │ 连接管理  │            │  证书验证    │
    └──────────┘            └──────────────┘
```

**为什么使用 PIMPL 模式？**

1. **编译隔离**：修改实现不触发依赖方的重新编译
2. **符号隐藏**：内部细节（libcurl/OpenSSL 头文件）不泄露到公共头文件
3. **ABI 稳定性**：私有成员变更不影响动态库导出符号
4. **项目一致性**：与 `Logger`、`YamlTool` 的架构风格完全统一

**为什么全部使用静态方法？**

1. **无状态操作**：HTTP 请求天然无状态（每次请求独立的 CURL 句柄）
2. **简化调用**：无需实例化，一行 `HttpsClient::get(url)` 即可
3. **线程安全**：无共享对象实例，仅全局配置通过 `std::atomic` 保护
4. **与 Logger 风格统一**：降低学习成本

### 2.3 线程安全设计

- **每次请求**：创建独立的 `CURL*` 句柄，互不干扰 → 天然线程安全
- **全局配置**：使用 `std::atomic<bool>` / `std::atomic<long>` 保证读写原子性
- **CA bundle 路径**：假定仅在初始化阶段设置一次，不频繁修改

> **注意**：由于使用独立的 libcurl 句柄，10 个线程同时发起 10 个 GET 请求不会阻塞。libcurl 本身是线程安全的。

---

## 3. API 参考

### HttpResponse

| 字段 | 类型 | 说明 |
|------|------|------|
| `statusCode` | `long` | HTTP 状态码（200, 404, 500, ...），0 表示未收到有效 HTTP 响应 |
| `body` | `std::string` | 响应体文本 |
| `headers` | `std::map<std::string, std::string>` | 响应头（所有 key 已转为小写，如 `content-type`） |
| `error` | `std::string` | 错误描述；请求成功时为空 |
| `ok()` | `bool` | `statusCode` 在 [200, 300) 且 `error` 为空时返回 `true` |

### GET 请求

```cpp
#include <httpclient/httpclient.h>

// 最简单用法
auto res = HttpsClient::get("https://api.example.com/data");
if (res.ok()) {
    std::cout << res.body << std::endl;
} else {
    std::cerr << "请求失败: " << res.error << " (HTTP " << res.statusCode << ")" << std::endl;
}

// 带自定义请求头
auto res2 = HttpsClient::get("https://api.example.com/data",
    {{"Authorization", "Bearer my-token-123"},
     {"Accept", "application/json"}});
```

### POST JSON 请求

```cpp
// Content-Type 自动设置为 application/json
auto res = HttpsClient::postJson("https://api.example.com/submit",
                                 R"({"name":"张三","age":30})",
                                 {{"Authorization", "Bearer my-token-123"}});

if (res.ok() && res.statusCode == 201) {
    std::cout << "创建成功: " << res.body << std::endl;
}
```

### POST 表单请求

```cpp
// Content-Type 自动设置为 application/x-www-form-urlencoded
// 键值自动 URL 编码
auto res = HttpsClient::postForm("https://api.example.com/login",
                                 {{"username", "admin"},
                                  {"password", "secret123"}});
```

### 全局配置

```cpp
// 设置超时（默认：总超时 30s，连接超时 10s）
HttpsClient::setTimeout(60);          // 总超时 60 秒
HttpsClient::setConnectTimeout(15);   // 连接建立超时 15 秒

// SSL 证书验证（默认：true，强烈建议保持开启）
HttpsClient::setVerifySsl(false);     // 仅调试用途！跳过服务器证书验证

// 指定 CA 证书包（Windows 上通常必须配置）
HttpsClient::setCaBundlePath("D:/certs/cacert.pem");
```

---

## 4. 使用前提条件

### 4.1 OpenSSL 运行时 DLL

| 平台 | 需要的文件 | 状态 |
|------|-----------|------|
| Windows MSVC | `libcrypto-3-x64.dll` + `libssl-3-x64.dll` | ✅ CMake 已配置自动拷贝到输出目录 |
| Linux | 静态链接 `.a` | ✅ 无运行时依赖 |

**无需手动操作**。CMake 构建系统已在 `cmake/3rd.cmake` 中配置 `copy_openssl_dlls` 目标，构建时自动把 DLL 拷贝到可执行文件同目录。

### 4.2 CA 根证书

**这是 Windows 上最容易被忽略的条件。** libcurl 在 Windows 上不自带根证书存储，需要你主动指定。

**问题现象**：
```
HTTPS 请求失败: SSL certificate problem: unable to get local issuer certificate
```

**解决方案（三选一）**：

**方案 A（推荐）：下载 CA bundle**
```cpp
// 1. 从 https://curl.se/ca/cacert.pem 下载 cacert.pem
// 2. 放到可执行文件同目录
// 3. 在程序启动时设置
HttpsClient::setCaBundlePath("cacert.pem");
```

**方案 B：使用 Windows 证书存储**（需额外代码，当前版本未内置）
```cpp
// 可以自行通过 libcurl 的 CURLSSLOPT_NATIVE_CA 实现
// 如有需求可扩展 HttpsClientPrivate
```

**方案 C：仅限调试——跳过验证**
```cpp
HttpsClient::setVerifySsl(false);  // ⚠️ 切勿在生产环境使用
```

**macOS / Linux 上通常无需额外配置**：libcurl 会使用系统证书存储（`/etc/ssl/certs/` 或 Keychain）。

### 4.3 服务端自签证书

内网环境中，服务器可能使用自签证书（非公开 CA 签发）。

**解决方案**：

1. **获取服务器的根证书**（PEM 格式）
2. **追加到 CA bundle 末尾**：
   ```bash
   cat server-root-ca.pem >> cacert.pem
   ```
3. **设置路径**：
   ```cpp
   HttpsClient::setCaBundlePath("cacert.pem");
   ```

### 4.4 客户端证书（mTLS）

某些高安全场景要求**双向 TLS 认证（mTLS）**：服务器不仅向客户端证明身份，客户端也需要提供证书。

当前版本 `HttpsClient` **未内置客户端证书支持**。如需使用：

- 方案 A：扩展 `HttpsClientPrivate`，添加 `CURLOPT_SSLCERT` / `CURLOPT_SSLKEY` 设置
- 方案 B：直接使用原始 libcurl API 处理特定的 mTLS 请求

### 4.5 Token / API Key

**Token 不属于 HTTPS 协议的范畴**，而是业务层的认证方式。

```cpp
// 通过 headers 参数传入
auto res = HttpsClient::get("https://api.example.com/data",
    {{"Authorization", "Bearer eyJhbGciOi..."}});   // Bearer Token

auto res2 = HttpsClient::get("https://api.example.com/data",
    {{"X-API-Key", "sk-abc123def456"}});             // API Key

auto res3 = HttpsClient::get("https://api.example.com/data",
    {{"Authorization", "Basic " + base64("user:pass")}}); // Basic Auth
```

`HttpsClient` 不关心 headers 的内容，直接透传给 libcurl。

---

## 5. 常见问题与排查

### Q1: "SSL certificate problem: unable to get local issuer certificate"

**原因**：libcurl 找不到根证书来验证服务器证书。

**解决**：参见 [4.2 CA 根证书](#42-ca-根证书)。

### Q2: "SSL certificate problem: certificate has expired"

**原因**：服务器证书已过期。

**解决**：联系服务器管理员更新证书。不能通过 `setVerifySsl(false)` 绕过——这是安全问题。

### Q3: "Couldn't resolve host name"

**原因**：DNS 解析失败。

**排查**：
- URL 拼写是否正确
- 是否有网络连接
- 是否需要配置代理（当前版本需手动设置 `CURLOPT_PROXY`）

### Q4: 请求超时

**原因**：网络延迟或服务器无响应。

**调整超时**：
```cpp
HttpsClient::setConnectTimeout(15);  // 连接超时 15s
HttpsClient::setTimeout(60);         // 总超时 60s
```

### Q5: Windows 上一切正常，Linux 上证书验证失败

**原因**：Linux 上可能缺少 CA 证书包。

**安装**：
```bash
# Debian/Ubuntu
sudo apt install ca-certificates

# RHEL/CentOS
sudo yum install ca-certificates

# 或手动指定 bundle 路径
HttpsClient::setCaBundlePath("/etc/ssl/certs/ca-certificates.crt");
```

### Q6: 如何发起 HTTP（非加密）请求？

`HttpsClient` 同样支持 `http://` URL。只需要移除 `s`：
```cpp
auto res = HttpsClient::get("http://localhost:8080/api/data");
```

---

## 附录：文件清单

```
新增/修改的文件：
├── common/include/common/httpclient/
│   ├── export.h               ← 导出宏
│   └── httpclient.h           ← 公共 API
├── common/private/httpclient/
│   ├── httpclient_p.h         ← 私有接口
│   └── httpclient_p.cpp       ← libcurl 实现
├── common/src/httpclient/
│   └── httpclient.cpp         ← PIMPL 转发层
├── common/CMakeLists.txt      ← [修改] 添加 HTTP_CLIENT_BUILDING_LIBRARY
├── test/main.cpp              ← [修改] 添加单元测试
└── doc/https_client_guide.md  ← 本文档
```
