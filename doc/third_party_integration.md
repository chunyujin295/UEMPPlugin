# 三方库集成方案

## 目录结构

```
3rd/
├── source/                          # 第三方源码
│   ├── spdlog-1.17.0/
│   ├── yaml-cpp-0.9.0/
│   ├── openssl-3.5.7/               #   仅用于一次性构建，不参与 CMake 编译
│   ├── openssl-cmake-3/             #   OpenSSL 的 CMake 封装层（ViaDuck 项目）
│   └── curl-8.21.0/
└── prebuilt/                        # 编译产物
    └── openssl/
        ├── mingw/                   #   MinGW 预编译包
        └── msvc/                    #   MSVC 预编译包
```

## CMake 架构

```
CMakeLists.txt                      全局配置 + include(cmake/3rd.cmake) + add_subdirectory
    │
    cmake/3rd.cmake                 ★ 所有三方依赖集中管理
    │   ├── spdlog        → add_subdirectory (静态库)
    │   ├── yaml-cpp      → add_subdirectory (静态库)
    │   ├── OpenSSL       → 直接创建 IMPORTED GLOBAL target
    │   │   └── openssl-cmake-3 → ssl / crypto INTERFACE target
    │   └── curl          → add_subdirectory (静态库)
    │
    common/CMakeLists.txt           只定义 common 库，链接已就绪的 target
    test/CMakeLists.txt             链接 common 的测试程序
```

## OpenSSL：一次性构建 + CMake 链接

### 为什么预编译

OpenSSL 的构建系统基于 `./Configure` + `make`，不原生支持 CMake。
在 Windows + MinGW 下尤其脆弱（Perl 版本、模块缺失、路径格式问题）。
因此采用 **一次性构建 → 预编译包 → CMake 直接链接** 策略。
客户机器上**不需要 Perl**。

### 构建（一次性）

**Linux** — 无需构建脚本，安装系统包即可：
```bash
sudo apt install libssl-dev     # Debian/Ubuntu
sudo dnf install openssl-devel  # Fedora
```

**MinGW** — Git Bash 中执行：
```bash
bash scripts/build_openssl_mingw.sh
```
脚本会先检查 `3rd/prebuilt/openssl/mingw/` 是否已存在目标版本：存在则跳过，
不存在则从 `3rd/source/openssl-3.5.7/` 源码构建（使用 `scripts/setup_perl_env.sh`
自动处理 Git MSYS2 Perl 模块缺失）。

**MSVC** — VS x64 Native Tools Command Prompt 中执行：
```bat
scripts\build_openssl_msvc.bat
```
优先尝试 vcpkg 获取二进制包，不可用则从源码构建（`perl Configure VC-WIN64A` →
`nmake` → `nmake install_sw`）。

### CMake 如何找到 OpenSSL

`cmake/3rd.cmake` 按平台采用不同策略：

**Windows** — 不依赖 CMake 内置 `FindOpenSSL`（避免 CMake 3.x / 4.x
行为差异），直接创建 IMPORTED GLOBAL target：

1. 设置 `OPENSSL_ROOT_DIR` → `3rd/prebuilt/openssl/<platform>`（`FORCE` 覆盖缓存值）
2. 按平台选择库文件名（MinGW: `.a`，MSVC: `.lib`）
3. 直接创建 `OpenSSL_SSL` / `OpenSSL_Crypto` 为 `STATIC IMPORTED GLOBAL`，
   显式指定 `IMPORTED_LOCATION` 和 `INTERFACE_INCLUDE_DIRECTORIES`
4. 创建 `OpenSSL::SSL` / `OpenSSL::Crypto` ALIAS
5. `add_subdirectory(openssl-cmake-3)` with `SYSTEM_OPENSSL=ON` →
   其内部 `find_package(OpenSSL)` 检测到 target 已存在，直接返回 →
   创建 `ssl` / `crypto` INTERFACE target 封装

**Linux** — 使用 CMake 内置 `find_package` 发现系统 OpenSSL，
然后 promote 为 IMPORTED GLOBAL（满足 curl 的 `try_compile` 需求）：

1. `find_package(OpenSSL REQUIRED)` → 系统路径（`/usr/lib/`, `/usr/include/`）
2. 将 `OpenSSL::SSL` / `OpenSSL::Crypto` 背后的 IMPORTED target 提升为 GLOBAL
3. `add_subdirectory(openssl-cmake-3)` → 同上，封装 `ssl` / `crypto`

### Target 依赖链

```
common
  PRIVATE
    ├── spdlog           (add_subdirectory 构建)
    ├── yaml-cpp         (add_subdirectory 构建)
    ├── ssl              (INTERFACE, openssl-cmake-3)
    │     └── OpenSSL::SSL (IMPORTED GLOBAL)
    │           └── libssl (Windows: prebuilt/, Linux: 系统 /usr/lib/)
    ├── crypto           (INTERFACE, openssl-cmake-3)
    │     └── OpenSSL::Crypto → libcrypto
    └── libcurl          (add_subdirectory 构建)
          └── OpenSSL::SSL + OpenSSL::Crypto (curl 自己的 target_link_libraries)
```

### curl 链接 OpenSSL 的关键点

1. **IMPORTED GLOBAL** — target 必须在根 scope 创建并设为 GLOBAL，
   否则 curl 的 `check_symbol_exists`（内部 `try_compile` 临时项目）找不到
2. **直接创建 Imported target** — 不经过 `find_package`，避免 CMake 版本差异
   导致 target 丢失
3. **ALIAS target 桥接** — `OpenSSL::SSL` 是 ALIAS，openssl-cmake-3 的
   `find_package` 和 curl 的 `find_package` 都能通过它找到真实 IMPORTED target

## 依赖关系总览

```
[一次性] Linux:  sudo apt install libssl-dev
         MinGW:  bash scripts/build_openssl_mingw.sh
         MSVC:   scripts\build_openssl_msvc.bat
              └── Windows: openssl 源码 → 3rd/prebuilt/openssl/<platform>/
                  Linux:   系统包管理器提供

[每次 CMake configure]
    cmake/3rd.cmake:
    ├── add_subdirectory(spdlog)       → target: spdlog
    ├── add_subdirectory(yaml-cpp)     → target: yaml-cpp
    ├── 直接创建 IMPORTED target       → OpenSSL::SSL / OpenSSL::Crypto
    │   └── add_subdirectory(openssl-cmake-3) → target: ssl / crypto
    └── add_subdirectory(curl)         → target: libcurl

[每次 CMake build]
    ├── 编译 spdlog + yaml-cpp (静态库)
    ├── 编译 curl (静态库, 链接 libssl + libcrypto)
    └── 编译 common (动态/静态库, 链接以上全部)

## 源码精简

原始 vendor 源码包含大量非构建必需的文件（测试、文档、CI 配置、
示例代码、替代构建系统等）。经过清理，`3rd/source/` 从 254M 削减到 90M。

### 各库清理明细

| 目录 | 原始 | 保留 | 削减 | 清理内容 |
|---|---|---|---|---|
| `openssl-3.5.7` | 219M | 75M | 144M | test (88M), 生成产物 .a/.dll (38M), doc (9.5M), fuzz (7.7M), demos, VMS, ms, 空 submodule 目录 |
| `curl-8.21.0` | 27M | 13M | 14M | tests (12M), docs (5.2M), src (curl CLI, 930K), projects, autotools, CI |
| `yaml-cpp-0.9.0` | 5.8M | 0.7M | 5.1M | test (4.9M), util, docs, bazel, CI |
| `spdlog-1.17.0` | 1.6M | 1.3M | 0.3M | tests, bench, example, logos, CI |
| `openssl-cmake-3` | 81K | 64K | 17K | CI yml, patches, scripts |
| **合计** | **254M** | **90M** | **164M (65%)** | |

### CMake 配套调整

清理目录后需要在 `cmake/3rd.cmake` 中做对应调整，避免 CMake 引用已被删除的子目录：

- `YAML_CPP_BUILD_TESTS=OFF` / `YAML_CPP_BUILD_TOOLS=OFF` — yaml-cpp 不再引用已删除的 `test/` / `util/`
- `BUILD_CURL_EXE=OFF` — curl 不再引用已删除的 `src/`（curl CLI）
- `BUILD_EXAMPLES=OFF` — curl 不再引用已删除的 `docs/examples/`
- `CMAKE_DISABLE_FIND_PACKAGE_Perl=ON`（curl 块内局部）— curl 不再尝试 `add_subdirectory(docs)`（man page 生成）。构建时会有 `Perl not found. Cannot build manuals.` 的 warning，属预期行为，可忽略
- `no-tests` 配置参数 — OpenSSL 构建脚本跳过测试

### 如果需要更新三方库版本

回退已删除内容的步骤：

1. 下载新版源码 tarball 到 `3rd/source/`
2. 从旧目录复制 `LICENSE` 文件（保留许可声明）
3. 按上表删除对应的目录和文件，保持与现有精简策略一致
4. 检查 `cmake/3rd.cmake` 中的选项是否需要调整

curl 的 `Perl not found` warning 是正常的——它只影响 man page 生成，
不影响库的编译和链接。curl 仍能正确链接 OpenSSL。
```
