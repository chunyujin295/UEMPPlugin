# UEMPPlugin

## 依赖库

| 库名          | 版本   | 用途                  | 集成方式   | 仓库地址                           |
| ------------- | ------ | --------------------- | ---------- | ---------------------------------- |
| libcurl       | 8.21.0 | HTTP/HTTPS 客户端     | 源码+CMake | https://github.com/curl/curl       |
| OpenSSL       | 3.5.7  | TLS/SSL 密码算法      | 预编译包   | https://github.com/openssl/openssl |
| spdlog        | 1.17.0 | 高性能日志库          | 源码+CMake | https://github.com/gabime/spdlog   |
| yaml-cpp      | 0.9.0  | YAML 配置文件解析     | 源码+CMake | https://github.com/jbeder/yaml-cpp |

## 目录结构

```
├── CMakeLists.txt              # 全局配置 → include(3rd.cmake) → add_subdirectory
├── cmake/
│   └── 3rd.cmake               # 所有三方依赖集中管理
├── common/                     # 核心库（logger + yamltool）
├── test/                       # 测试项目
├── scripts/                    # 构建脚本
│   ├── build_openssl_mingw.sh  #   编译 MinGW 版 OpenSSL→prebuilt
│   ├── build_openssl_msvc.bat  #   编译 MSVC 版 OpenSSL→prebuilt
│   └── setup_perl_env.sh       #   MSYS2 Perl 环境初始化
├── 3rd/
│   ├── source/                 #   第三方源码
│   │   ├── spdlog-1.17.0/
│   │   ├── yaml-cpp-0.9.0/
│   │   ├── curl-8.21.0/
│   │   ├── openssl-3.5.7/
│   │   └── openssl-cmake-3/
│   └── prebuilt/               #   预编译产物
│       └── openssl/
│           ├── mingw/
│           └── msvc/
└── doc/
    └── third_party_integration.md  # 三方库集成方案详细文档
```

## 构建

**Linux**:
```bash
# 安装系统依赖
sudo apt install libssl-dev    # Debian/Ubuntu
# 或
sudo dnf install openssl-devel # Fedora

# CMake 构建
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```

**MinGW** (Windows, Git Bash):
```bash
# 首次：编译 OpenSSL 预编译包（只需一次）
bash scripts/build_openssl_mingw.sh

# 日常 CMake 构建
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```

**MSVC** (Windows, VS Developer Command Prompt):
```bat
REM 首次：编译 OpenSSL 预编译包（只需一次）
scripts\build_openssl_msvc.bat

REM 日常 CMake 构建
cmake -B build -S . -DBUILD_TEST=ON
cmake --build build
```

三方库集成的详细方案见 [doc/third_party_integration.md](doc/third_party_integration.md)。
