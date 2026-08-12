# Linux 构建与运行指南

## 目录

- [环境要求](#环境要求)
- [快速开始](#快速开始)
- [构建指定 Target](#构建指定-target)
- [运行指定测试](#运行指定测试)
- [目标一览](#目标一览)
- [常见问题](#常见问题)

---

## 环境要求

| 工具 | 最低版本 | 说明 |
|------|----------|------|
| CMake | 3.21+ | 构建系统 |
| GCC / Clang | C++17 | 编译器 |
| Perl | — | OpenSSL 源码构建需要（CMake 自动触发） |
| Python | 3.7+ (可选) | `test_httpclient` 的本地 HTTPS 集成测试需要 |

Perl 通常系统自带；如未安装：

```bash
# Ubuntu / Debian
sudo apt install perl

# CentOS / RHEL / Fedora
sudo dnf install perl

# Arch
sudo pacman -S perl
```

---

## 快速开始

```bash
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```

首次 configure 时 CMake 自动从 `3rd/source/openssl-3.5.7` 构建 OpenSSL 静态库并安装到 `3rd/lib/openssl/Linux/`。后续构建直接复用该目录下的 `.a` 文件，无需重复构建。

---

## 构建指定 Target

### 语法

```bash
cmake --build build --target <target_name>
```

### 构建库

```bash
# 仅编译 common 共享库
cmake --build build --target common
```

### 构建单个测试

```bash
cmake --build build --target test_logger
cmake --build build --target test_yamltool
cmake --build build --target test_openssl
cmake --build build --target test_curl
cmake --build build --target test_httpclient
cmake --build build --target test_json
```

### 构建多个测试（并行）

```bash
cmake --build build --parallel 4 --target test_logger test_json test_curl
```

### 常见工作流

```bash
# 场景：只改了 logger 相关代码，只编译和运行它
cmake --build build --target test_logger
./build/out/test_logger

# 场景：改了 common 库，重新编译所有依赖它的测试
cmake --build build --target common          # 先编译库
cmake --build build --target test_logger test_yamltool test_httpclient  # 再编译受影响的测试

# 场景：改动了 curl 集成，只验证 curl 和 httpclient
cmake --build build --target test_curl test_httpclient
```

> **提示：** `--target` 指定的是 CMake target 名（不含前缀后缀），不是文件名。构建时 CMake 自动解析 target 的依赖链——例如编译 `test_logger` 会自动先编译它依赖的 `common`（如果 `common` 有改动）。

---

## 运行指定测试

### 直接运行可执行文件

Linux 下产物**没有 `.exe` 后缀**，直接运行二进制文件即可：

```bash
./build/out/test_logger
./build/out/test_yamltool
./build/out/test_openssl
./build/out/test_curl
./build/out/test_httpclient
./build/out/test_json
```

### 通过 CTest 运行

```bash
cd build && ctest
```

**按名称筛选单个或一组测试：**

```bash
ctest -R logger          # 只跑 test_logger
ctest -R httpclient      # 只跑 test_httpclient
ctest -R json            # 只跑 test_json
```

`-R` 接受正则，可一次匹配多个：

```bash
ctest -R "logger|yamltool"            # 跑 logger + yamltool
ctest -R "test_"                      # 跑全部（等价于 ctest 无参数）
```

**按名称排除：**

```bash
ctest -E httpclient    # 跳过 httpclient，跑其余全部
```

**额外选项：**

```bash
ctest -V                # --verbose：打印所有测试输出
ctest -j4               # 并行跑 4 个测试
ctest --output-on-failure   # 只打印失败测试的输出
ctest --rerun-failed    # 只重跑上一轮失败的测试
```

### 一键构建 + 运行

```bash
# 构建并运行单个测试的快捷方式
cmake --build build --target test_logger && ./build/out/test_logger

# 封装成函数（添加到 ~/.bashrc）
run_test() { cmake --build build --target "test_$1" && ./build/out/"test_$1"; }
# 用法：run_test logger   → 等价于上方的两行命令
```

---

## 目标一览

### 库

| CMake Target | 产物 | 说明 |
|---|---|---|
| `common` | `build/out/libcommon.so` | 核心共享库（logger + yamltool + httpclient） |

### 测试

| CMake Target | 产物 | CTest 名称 | 依赖 |
|---|---|---|---|
| `test_logger` | `build/out/test_logger` | `logger` | `common` |
| `test_yamltool` | `build/out/test_yamltool` | `yamltool` | `common` |
| `test_openssl` | `build/out/test_openssl` | `openssl` | `OpenSSL::SSL` `OpenSSL::Crypto` |
| `test_curl` | `build/out/test_curl` | `curl` | `libcurl` |
| `test_httpclient` | `build/out/test_httpclient` | `httpclient` | `common` `OpenSSL::SSL` `OpenSSL::Crypto` `libcurl` |
| `test_json` | `build/out/test_json` | `json` | `nlohmann_json` (header-only) |

---

## Linux 特定说明

### 共享库路径

`common` 编译为共享库（`libcommon.so`），直接运行测试可执行文件时需要系统能找到它。

**方法一（推荐开发时）：** 设置 `LD_LIBRARY_PATH`

```bash
export LD_LIBRARY_PATH=$(pwd)/build/out:$LD_LIBRARY_PATH
./build/out/test_logger
```

**方法二：** 使用 `-rpath` 嵌入运行时搜索路径

在 configure 时添加：

```bash
cmake -B build -S . -DBUILD_TEST=ON \
    -DCMAKE_INSTALL_RPATH="\$ORIGIN" \
    -DCMAKE_BUILD_RPATH="\$ORIGIN"
```

`$ORIGIN` 表示"可执行文件所在目录"，这样测试程序会在 `build/out/` 下自动找到 `libcommon.so`。

**方法三（构建期设置）：** 在 `CMakeLists.txt` 中为所有 target 设置 `BUILD_RPATH`

如果你希望合入仓库，可在根 `CMakeLists.txt` 添加（团队统一方案）：

```cmake
set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/out")
```

### 静态链接备选

如果不想处理共享库路径问题，可在 configure 时关闭共享构建，把所有代码链接成静态测试程序：

```bash
cmake -B build -S . -DBUILD_TEST=ON -DCOMMON_BUILD_SHARED=OFF
cmake --build build --target test_logger
./build/out/test_logger          # 无需 LD_LIBRARY_PATH
```

### OpenSSL 构建

首次 `cmake -B build` 时，`cmake/3rd.cmake` 检测 `3rd/lib/openssl/Linux/lib/libssl.a` 是否存在：
- **存在** → 直接使用预编译库
- **不存在** → 自动调用 `scripts/build_openssl.sh` 从源码构建并安装

如需重建 OpenSSL：

```bash
rm -rf 3rd/lib/openssl/Linux
cmake -B build -S . -DBUILD_TEST=ON
```

---

## 常见问题

### Q: 运行测试时报 `libcommon.so: cannot open shared object file`

```bash
export LD_LIBRARY_PATH=$(pwd)/build/out:$LD_LIBRARY_PATH
```

或参考上文[共享库路径](#共享库路径)的永久方案。

### Q: `cmake --build build --target xxx` 提示 `No target xxx`

确认 configure 时开了 `-DBUILD_TEST=ON`。检查 target 名是否拼写正确：

```bash
# 列出所有可用 target
cmake --build build --target help
```

### Q: CTest 提示 `No tests were found`

重新 configure 确保 `BUILD_TEST=ON`：

```bash
cmake -B build -S . -DBUILD_TEST=ON
```

### Q: `test_httpclient` 在 Linux 下跳过 HTTPS 集成测试

确保 Python 3 可用且路径中没有 Windows Store 桩程序的问题（这是 Windows 特有的，Linux 下通常不会遇到）：

```bash
python3 --version   # 确认存在
```

如果测试仍跳过，查看具体输出：

```bash
./build/out/test_httpclient
```

### Q: 如何查看编译详细信息

```bash
cmake --build build --verbose          # 显示完整编译命令
cmake --build build --target test_curl VERBOSE=1   # 对单个 target 显示详细信息
```

### Q: CI 中如何最快地构建并跑完所有测试

```bash
cmake -B build -S . -DBUILD_TEST=ON -DCOMMON_BUILD_SHARED=OFF
cmake --build build -j$(nproc)
cd build && ctest -j$(nproc) --output-on-failure
```
