# 三方库集成方案

## 平台支持

| 平台 | OpenSSL 构建 | 脚本 |
|------|-------------|------|
| Windows MSVC | `perl Configure VC-WIN64A` → `nmake` → `nmake install_sw` | `scripts/build_openssl_msvc.bat` |
| Linux | `./Configure` → `make` → `make install_sw` | `scripts/build_openssl.sh` |
| Windows MinGW | 未支持 | — |

MinGW 未支持的原因：OpenSSL 的 `./Configure mingw64` 要求 MSYS2 风格的 Perl（产生 Unix 路径），
但 Git for Windows 自带的 MSYS2 Perl 缺少核心模块，且即使补充模块后仍有 MSYS2 路径转换等兼容性问题。

## 目录结构

```
3rd/
├── source/                    # 第三方源码
│   ├── spdlog-1.17.0/
│   ├── yaml-cpp-0.9.0/
│   ├── openssl-3.5.7/         # 仅用于一次性构建，不参与 CMake 编译
│   └── curl-8.21.0/
└── lib/                       # 预编译静态库（版本控制，克隆即用）
    └── openssl/
        ├── MSVC/              #   include/ + lib/libssl.lib + libcrypto.lib
        └── Linux/             #   include/ + lib/libssl.a + libcrypto.a
```

## CMake 架构

```
CMakeLists.txt                       全局配置 + include(cmake/3rd.cmake) + add_subdirectory
    │
    cmake/3rd.cmake                  ★ 所有三方依赖集中管理
    │   ├── spdlog        → add_subdirectory (静态库, target: spdlog)
    │   ├── yaml-cpp      → add_subdirectory (静态库, target: yaml-cpp)
    │   ├── OpenSSL       → execute_process 调用构建脚本 → IMPORTED GLOBAL target
    │   └── curl          → add_subdirectory (静态库, target: libcurl)
    │
    cmake/FindOpenSSL.cmake          curl 的 find_package(OpenSSL) 拦截 shim
    │
    common/CMakeLists.txt            只定义 common 库，链接已就绪的 target
    test/CMakeLists.txt              链接 common + OpenSSL::SSL/Crypto + libcurl
```

## OpenSSL 构建

### CMake 集成流程

`cmake/3rd.cmake` 中的 OpenSSL 块：

1. 检查 `3rd/lib/openssl/<platform>/lib/` 下是否存在预编译库
2. 存在 → 直接使用（仓库已包含，克隆即用）
3. 不存在 → `execute_process` 调用构建脚本从源码编译并安装到此目录（一次性）
3. 创建 `OpenSSL_SSL` / `OpenSSL_Crypto` 为 `STATIC IMPORTED GLOBAL`
4. 创建 `OpenSSL::SSL` / `OpenSSL::Crypto` ALIAS
5. 设置 `OPENSSL_FOUND=TRUE`（配合 `cmake/FindOpenSSL.cmake` 拦截 curl 的重复搜索）

### 构建命令（来自官方 INSTALL.md）

**MSVC** (VS Developer Command Prompt):
```
perl Configure VC-WIN64A --prefix=<prefix> --openssldir=<prefix>/ssl --libdir=lib no-tests ...
nmake
nmake install_sw
```

**Linux**:
```
./Configure --prefix=<prefix> --openssldir=<prefix>/ssl --libdir=lib no-tests ...
make -j$(nproc)
make install_sw
```

### Target 依赖链

```
common
  PRIVATE
    ├── spdlog               (add_subdirectory)
    ├── yaml-cpp             (add_subdirectory)
    ├── OpenSSL::SSL         (IMPORTED GLOBAL, cmake/3rd.cmake 创建)
    │     └── libssl         (build/openssl/lib/)
    ├── OpenSSL::Crypto      (IMPORTED GLOBAL)
    │     └── libcrypto
    └── libcurl              (add_subdirectory)
          └── OpenSSL::SSL + OpenSSL::Crypto
```

### curl 集成

#### find_package 拦截

curl 的 CMakeLists.txt 中有 `find_package(OpenSSL REQUIRED)`，会触发 CMake 内置的
`FindOpenSSL` 模块去搜索系统中安装的 OpenSSL。如果不拦截，有两个问题：

1. **找到错误的 OpenSSL** — 系统中可能存在其他版本的 OpenSSL（如 StrawberryPerl 自带 3.3.0），
   curl 会链接那个而不是我们编译的
2. **找不到直接报错** — `REQUIRED` 会让 CMake 直接 `FATAL_ERROR`

解决方案：`cmake/FindOpenSSL.cmake`。`cmake/3rd.cmake` 将 `cmake/` 加入 `CMAKE_MODULE_PATH`
后，CMake 搜索 Find 模块时会优先找到我们的 `FindOpenSSL.cmake` 而非内置版本。
它的逻辑很简单：

```cmake
# cmake/FindOpenSSL.cmake
if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
    set(OPENSSL_FOUND TRUE)   # 告诉 curl "找到了"
    return()                  # 不触发内置 FindOpenSSL 的搜索
endif()
# 兜底：如果 target 不存在（理论上不会），走 CMake 内置的正常搜索流程
include(${CMAKE_ROOT}/Modules/FindOpenSSL.cmake)
```

因为 `cmake/3rd.cmake` 在 curl 的 `add_subdirectory` 之前就已经创建了
`OpenSSL::SSL` 和 `OpenSSL::Crypto`，所以 shim 一定会走 `return()` 分支。

#### 其他 curl 配置

## curl 配置

```cmake
CURL_ENABLE_SSL ON        # 启用 SSL/TLS
CURL_USE_OPENSSL ON       # 使用 OpenSSL 后端
BUILD_CURL_EXE OFF        # 不构建 CLI
CURL_DISABLE_TESTS ON     # 不构建测试（测试目录已从源码中删除）
CURL_DISABLE_SRP ON       # TLS-SRP 已通过 no-srp 禁用
```

## 依赖关系总览

```
[一次性] MSVC:  scripts\build_openssl_msvc.bat <prefix>
         Linux: bash scripts/build_openssl.sh <prefix>
              └── openssl 源码 → <prefix>/lib/

[每次 CMake configure]
    cmake/3rd.cmake:
    ├── add_subdirectory(spdlog)       → target: spdlog
    ├── add_subdirectory(yaml-cpp)     → target: yaml-cpp
    ├── 构建 OpenSSL（如不存在）      → IMPORTED GLOBAL target
    │   └── OpenSSL::SSL / OpenSSL::Crypto
    └── add_subdirectory(curl)         → target: libcurl
          └── find_package(OpenSSL) → FindOpenSSL.cmake shim → 复用已有 target

[每次 CMake build]
    ├── 编译 spdlog + yaml-cpp (静态库)
    ├── 编译 curl (静态库, 链接 libssl + libcrypto)
    └── 编译 common (动态/静态库, 链接以上全部)
```

## 源码精简

原始 vendor 源码包含大量非构建必需的文件（测试、文档、CI 配置等）。
经过清理，`3rd/source/` 从 254M 削减到 ~90M。

| 目录 | 原始 | 保留 | 削减 | 清理内容 |
|---|---|---|---|---|
| `openssl-3.5.7` | 219M | ~75M | ~144M | test, fuzz, doc, demos, VMS, ms, 生成产物 |
| `curl-8.21.0` | 27M | ~13M | ~14M | tests, docs, src(cli), projects, autotools, CI |
| `yaml-cpp-0.9.0` | 5.8M | ~0.7M | ~5.1M | test, util, docs, bazel, CI |
| `spdlog-1.17.0` | 1.6M | ~1.3M | ~0.3M | tests, bench, example, logos, CI |

### CMake 配套调整

- `YAML_CPP_BUILD_TESTS=OFF` / `YAML_CPP_BUILD_TOOLS=OFF` — 不引用已删除的 test/util
- `BUILD_CURL_EXE=OFF` — 不构建已删除的 src(cli)
- `CURL_DISABLE_TESTS=ON` — 不引用已删除的 tests
- `ENABLE_CURL_MANUAL=OFF` / `BUILD_LIBCURL_DOCS=OFF` — 不引用已删除的 docs
- `openssl-3.5.7/build.info` 移除了 `fuzz doc` — Configure 不再引用已删除的目录
