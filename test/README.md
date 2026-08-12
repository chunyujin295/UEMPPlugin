# 测试

## 测试目标

每个模块一个可执行文件，分别注册为 CTest 测试。

| 测试 | 源文件 | 覆盖范围 |
|------|--------|----------|
| logger | `test_logger.cpp` | 日志写入、文件输出、级别过滤 |
| yamltool | `test_yamltool.cpp` | YAML 解析、节点访问 |
| openssl | `test_openssl.cpp` | 版本号、RAND、EVP、SSL/TLS 基础 |
| curl | `test_curl.cpp` | HTTP GET/POST、自定义 Header、HTTPS 验证 |
| httpclient | `test_httpclient.cpp` | HttpClient 封装、本地 HTTPS 服务端集成 |
| json | `test_json.cpp` | 基本类型、解析/序列化、数组、STL 互操作、嵌套路径、异常处理 |

## 公共测试基础设施

`test_common.h` 提供 `TestRunner` 类，用于计数断言和失败数：

```cpp
struct TestRunner {
    int asserts  = 0;
    int failures = 0;

    void check(bool cond, const char* msg);  // 自动打印 PASS/FAIL
    int  finish();  // 打印统计，返回 0（全通过）或 1（有失败）
};
```

## 构建与运行

```bash
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build

# 运行全部测试
cd build && ctest

# 运行单个测试
./out/test_json.exe
```

## nlohmann/json 注意事项

- **`value(key, default)` 仅在 key 缺失时返回默认值**，类型不匹配会抛 `type_error.302`。
- **`value(key, default)` 要求对象类型**，null json 上调用会抛 `type_error.306`。
- **`update()` 是覆盖语义**，同名键会被覆写，不是"仅补缺"。
