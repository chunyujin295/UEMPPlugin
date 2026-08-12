# UEMPPlugin

## 依赖库

| 库名    | 版本   | 用途              | 集成方式 |
|---------|--------|-------------------|---------|
| libcurl | 8.21.0 | HTTP/HTTPS 客户端  | 源码 + CMake `add_subdirectory` |
| OpenSSL | 3.5.7  | TLS/SSL 密码算法  | 源码, CMake `execute_process` 调用构建脚本 |
| spdlog  | 1.17.0 | 高性能日志库      | 源码 + CMake `add_subdirectory` |
| yaml-cpp| 0.9.0  | YAML 配置文件解析  | 源码 + CMake `add_subdirectory` |

## 平台支持

| 平台 | 状态 | OpenSSL 构建方式 |
|------|------|-----------------|
| Windows MSVC | 支持 | `scripts/build_openssl_msvc.bat` (CMake 自动触发) |
| Linux | 支持 | `scripts/build_openssl.sh` (CMake 自动触发) |
| Windows MinGW | 未支持 | MinGW 下构建 OpenSSL 需要 MSYS2 Perl 环境，复杂且不可靠 |

## 目录结构

```
├── CMakeLists.txt            # 全局配置 → include(3rd.cmake) → add_subdirectory
├── cmake/
│   ├── 3rd.cmake             # 所有三方依赖集中管理
│   └── FindOpenSSL.cmake     # curl 的 find_package(OpenSSL) 拦截 shim
├── common/                   # 核心库（logger + yamltool + httpclient）
├── test/                     # 测试项目
├── scripts/
│   ├── build_openssl.sh      # Linux 下 OpenSSL 源码构建
│   └── build_openssl_msvc.bat# MSVC 下 OpenSSL 源码构建
├── 3rd/
│   ├── source/               # 第三方源码
│   │   ├── spdlog-1.17.0/
│   │   ├── yaml-cpp-0.9.0/
│   │   ├── curl-8.21.0/
│   │   └── openssl-3.5.7/
│   └── lib/                  # 预编译库（版本控制，克隆即用）
│       └── openssl/
│           ├── MSVC/         #   bin/ (DLL) + lib/ (.lib) + include/
│           └── Linux/        #   lib/ (.a) + include/
└── doc/
    ├── third_party_integration.md  # 三方库集成详细文档
├── https_client_guide.md       # HTTPS 客户端文档（技术原理 + API）
```

## 构建

**Windows (MSVC)** — VS Developer Command Prompt:
```bat
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```
OpenSSL 预编译库在 `3rd/lib/openssl/<platform>/` 下，已上传仓库，克隆后直接可用。
如缺失（或版本不匹配），CMake 首次 configure 时自动从源码构建。

**Linux**:
```bash
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```
CMake 首次 configure 时自动从 `3rd/source/openssl-3.5.7` 构建 OpenSSL。

三方库集成的详细方案见 [doc/third_party_integration.md](doc/third_party_integration.md)。
