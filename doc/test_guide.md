# 测试指南

## 目录结构

```
test/
  test_common.h         ← 共享测试基础设施（TestRunner 结构体）
  test_logger.cpp       ← Logger 测试
  test_yamltool.cpp     ← YamlTool 测试
  test_openssl.cpp      ← OpenSSL 测试
  test_curl.cpp         ← libcurl 测试
  test_httpclient.cpp   ← HttpsClient 测试
  test_json.cpp         ← nlohmann/json 测试
  https_server/         ← 本地 HTTPS 测试服务器
    server.py           ← Python echo 服务器
    cert.pem            ← 自签名证书（CN=localhost）
    key.pem             ← 私钥
  CMakeLists.txt        ← 6 个独立 test executable + CTest 注册
```

## 构建与运行

### 构建

```bash
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```

构建产物统一输出到 `build/out/`，CMake 在编译期自动完成：

| 步骤 | 目标 | 内容 |
|---|---|---|
| POST_BUILD | `test_curl`, `test_httpclient` | 拷贝 `3rd/cacert.pem` → `out/` |
| POST_BUILD | `test_httpclient` | 拷贝 `test/https_server/` → `out/` |
| compile | `test_httpclient` | 注入 `HTTPS_SERVER_DIR` 宏为 `https_server/` 的绝对路径 |

### 运行

**单个模块：**

```bash
./build/out/test_logger.exe
./build/out/test_yamltool.exe
./build/out/test_openssl.exe
./build/out/test_curl.exe
./build/out/test_httpclient.exe
./build/out/test_json.exe
```

**全部模块（CTest）：**

```bash
cd build && ctest
```

或按名称筛选：

```bash
ctest -R httpclient
ctest -R json
```

## 测试模块一览

### 1. test_logger — Logger

| 测试 | 说明 |
|---|---|
| `test_logger_levels` | 六个日志等级调用不崩溃 |
| `test_logger_set_config_path` | setConfigPath 创建配置文件 |
| `test_logger_callback` | 回调 sink 正确接收日志 |

**依赖：** `common` 库（内部依赖 spdlog + yaml-cpp）

---

### 2. test_yamltool — YamlTool

| 测试 | 说明 |
|---|---|
| `test_yamltool_basic` | int / string / double / bool 读写回环；getDef 默认值；setDef 语义 |
| `test_yamltool_node_types` | Map / Sequence / Null 类型判断；序列索引访问 |
| `test_yamltool_set_null` | setNull 强制写入、setNullDef 条件写入 |

**依赖：** `common` 库（内部依赖 yaml-cpp）

---

### 3. test_openssl — OpenSSL

| 测试 | 说明 |
|---|---|
| `test_openssl_version` | 版本号 ≥ 3.5.0 |
| `test_openssl_sha256` | SHA256 哈希计算及确定性验证 |
| `test_openssl_random` | RAND_bytes 生成随机数，两次调用不重复 |

**依赖：** `OpenSSL::SSL` `OpenSSL::Crypto`（静态导入库）

---

### 4. test_curl — libcurl

| 测试 | 说明 |
|---|---|
| `test_curl_version` | 版本信息获取，验证 HTTPS 协议支持 |
| `test_curl_init_cleanup` | 初始化 / 清理不崩溃 |
| `test_curl_https_get` | HTTPS GET 到 `httpbin.org/get`（无网络时自动跳过） |

**CA 证书：** 使用 `out/cacert.pem`（Mozilla CA 包，CMake POST_BUILD 自动拷贝）

**依赖：** `libcurl` 静态库

---

### 5. test_httpclient — HttpsClient

测试分两阶段运行：

**Phase 1 — 无需网络（纯 struct / config / 错误分支）：**

| 测试 | 说明 |
|---|---|
| `test_httpsclient_response_ok` | `HttpResponse::ok()` 逻辑（200、404、网络错误） |
| `test_httpsclient_config` | 全局配置 setter（超时 / SSL 验证 / CA bundle） |
| `test_httpsclient_invalid_url` | 无效域名 → `ok() == false` + 错误消息非空 |

**Phase 2 — 本地 HTTPS echo 服务器集成测试：**

测试启动本地 Python HTTPS 服务器（`test/https_server/server.py`），然后：

| 测试 | 说明 |
|---|---|
| `test_httpsclient_get` | GET `/get` → 验证状态码 200、响应体含 method/GET、headers 非空 |
| `test_httpsclient_get_with_headers` | 自定义 `X-Custom-Header` → 服务器回显 |
| `test_httpsclient_post_json` | POST `/post` JSON → 状态码 200、回显 hello/world/count |
| `test_httpsclient_post_form` | POST `/post` form-urlencoded → 状态码 200、回显表单字段 |

**依赖：** `common` `OpenSSL::SSL` `OpenSSL::Crypto` `libcurl`

---

### 6. test_json — nlohmann/json

| 测试 | 说明 |
|---|---|
| `test_json_basic_types` | is_string / is_number / is_boolean 类型判断，get 取值 |
| `test_json_value_with_default` | `json::value(key, default)` 查缺补默认值，类型不匹配回退 |
| `test_json_parse_serialize` | `json::parse()` → `dump()` → `parse()` 回环一致性 |
| `test_json_array_operations` | push_back 追加，operator[] 访问，range-for 遍历 |
| `test_json_stl_interop` | `std::vector<int>` ↔ json，`std::map<string,int>` ↔ json |
| `test_json_nested_and_paths` | 嵌套对象自动创建，`contains()`，JSON Pointer (`_json_pointer`) |
| `test_json_dump_formatting` | `dump()` 紧凑输出，`dump(4)` 缩进输出 |
| `test_json_parse_error_handling` | 非法 JSON / 空字符串 → 抛出 `json::parse_error` |
| `test_json_null_and_empty` | 默认构造 / nullptr / object() / array() 的 null/empty 语义 |
| `test_json_update_and_merge` | `emplace` 插入、`update` 递归合并、exist 保护语义 |
| `test_json_comparison` | `operator==` / `operator!=` 对对象和标量 |

**依赖：** `nlohmann_json`（header-only，仅需引入 include 路径）

---

## 本地 HTTPS 测试服务器

### 前提条件

- **Python 3.7+**（使用标准库 `http.server` + `ssl` 模块，无需 pip）
- **自签名证书**（`test/https_server/cert.pem` + `key.pem`，已随仓库提供）

### 生成自签名证书

```bash
MSYS_NO_PATHCONV=1 openssl req -x509 -newkey rsa:2048 \
  -keyout test/https_server/key.pem \
  -out   test/https_server/cert.pem \
  -days 36500 -nodes \
  -subj "/CN=localhost/O=UEMPPlugin Test/C=CN"
```

### 工作原理

```
test_httpclient.exe
    │
    ├─ _popen("py -3 .../https_server/server.py")
    │   └─ server.py 打印 "READY:<port>" 到 stdout
    │
    ├─ HttpsClient::setVerifySsl(false)    ← 自签名证书，跳过验证
    │
    ├─ GET  https://localhost:<port>/get
    ├─ GET  https://localhost:<port>/get   (+ custom headers)
    ├─ POST https://localhost:<port>/post  (JSON)
    ├─ POST https://localhost:<port>/post  (form-urlencoded)
    │
    └─ GET  https://localhost:<port>/shutdown  → 服务器优雅退出
```

### Python 查找策略

测试代码依次尝试 `python3` 和 `python`，先用 `python -c "print('ok')"` 做**存活探测**——只有实际产生 stdout 输出的解释器才会被使用。

这对 Windows 尤其重要：`C:\Users\<user>\AppData\Local\Microsoft\WindowsApps\` 下的 `python.exe` / `python3.exe` 是 Windows Store 桩程序，`_popen` 能启动它们但不会有任何 stdout 输出（它们弹出一个 Store 对话框）。存活探测精准过滤掉这些桩。

> **注意：** 如果刚添加了 Python 到系统 PATH，需要**重启 IDE / 终端**，否则子进程继承的是旧环境变量。

### 通信协议

`server.py` 所有输出均写入 **stdout**（不使用 stderr），与 C++ 测试 harness 的约定：

| 输出行 | 含义 |
|---|---|
| `READY:<port>` | 服务器已就绪，监听 127.0.0.1:`<port>` |
| `FATAL:<reason>` | 启动失败（证书缺失、端口占用等） |

`fgets` 读到 `READY:` 则测试继续，读到其他输出则打印完整诊断后跳过集成测试。

### 服务器端点

| 端点 | 方法 | 响应 |
|---|---|---|
| `/get` | GET | `{"method":"GET","path":"/get","headers":{...}}` |
| `/post` | POST | `{"method":"POST","path":"/post","headers":{...},"body":"..."}` |
| `/shutdown` | GET | `ok`（纯文本），触发 `server.shutdown()` |

---

## 测试基础设施

### TestRunner

`test_common.h` 提供了 `TestRunner` 结构体，每个 test executable 独立实例化一份：

```cpp
struct TestRunner {
    int asserts  = 0;   // 总断言数
    int failures = 0;   // 失败断言数

    void check(bool cond, const char* msg);  // 断言 + 打印 PASS/FAIL
    int  finish();                            // 打印统计，返回 0=成功 1=失败
};
```

### 设计原则

1. **独立 executable** — 每个模块一个 `test_*.exe`，链接最少的第三方依赖，互不干扰
2. **最小依赖** — 如 `test_json` 只链接 `nlohmann_json`（header-only 宏），不拖 `common`
3. **内网可跑** — HTTP 集成测试使用本地泛用型 echo 服务器，不依赖外网
4. **代码即文档** — 测试覆盖了每个公共 API 的 happy path、corner case 和错误分支
