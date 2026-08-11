@echo off
REM Build/acquire OpenSSL 3.5.x for MSVC (x64) and install to prebuilt directory.
REM Run from the project root in a Visual Studio x64 Native Tools Command Prompt:
REM   scripts\build_openssl_msvc.bat
REM
REM Strategy: try vcpkg first, fall back to building from source.

setlocal enabledelayedexpansion

set "PROJECT_DIR=%~dp0.."
set "PREFIX=%PROJECT_DIR%\3rd\prebuilt\openssl\msvc"

echo === OpenSSL MSVC Prebuilt Setup ===
echo Target: %PREFIX%

REM Clean previous install
if exist "%PREFIX%" rmdir /s /q "%PREFIX%"
mkdir "%PREFIX%"

REM ---- Option 1: vcpkg ----
set "VCPKG_FOUND=0"
for /f "delims=" %%i in ('where vcpkg 2^>nul') do set "VCPKG_EXE=%%i"
if defined VCPKG_EXE (
    echo vcpkg found: %VCPKG_EXE%
    echo Installing OpenSSL via vcpkg...
    vcpkg install openssl:x64-windows-static
    if !ERRORLEVEL! EQU 0 (
        REM Copy from vcpkg installed tree to our prebuilt directory
        for /f "tokens=*" %%d in ('vcpkg root') do set "VCPKG_ROOT=%%d"
        if exist "!VCPKG_ROOT!\installed\x64-windows-static\include\openssl" (
            echo Copying from vcpkg installed tree...
            xcopy /e /y "!VCPKG_ROOT!\installed\x64-windows-static\include\openssl\*" "%PREFIX%\include\openssl\" >nul
            copy /y "!VCPKG_ROOT!\installed\x64-windows-static\lib\libssl.lib"   "%PREFIX%\lib\" >nul
            copy /y "!VCPKG_ROOT!\installed\x64-windows-static\lib\libcrypto.lib" "%PREFIX%\lib\" >nul
            copy /y "!VCPKG_ROOT!\installed\x64-windows-static\bin\*.dll"         "%PREFIX%\bin\" >nul 2>nul
            set "VCPKG_FOUND=1"
            echo Done via vcpkg.
        )
    )
)

if "%VCPKG_FOUND%"=="1" goto :done

REM ---- Option 2: Build from source ----
echo vcpkg not available, building OpenSSL from source...
set "SRC=%PROJECT_DIR%\3rd\source\openssl-3.5.7"

where perl  >nul 2>&1 || (echo ERROR: perl not found in PATH & exit /b 1)
where nmake >nul 2>&1 || (echo ERROR: nmake not found - run from VS Developer Command Prompt & exit /b 1)
where cl    >nul 2>&1 || (echo ERROR: cl.exe not found - run from VS Developer Command Prompt & exit /b 1)

cd /d "%SRC%"
if exist Makefile nmake clean 2>nul

echo Configuring OpenSSL for VC-WIN64A...
perl Configure VC-WIN64A ^
    --prefix="%PREFIX%" ^
    --openssldir="%PREFIX%\ssl" ^
    no-tests ^
    no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 ^
    no-engine no-idea no-camellia no-ssl3 ^
    no-heartbeats no-gost no-deprecated ^
    no-comp no-dtls no-psk no-srp no-dso no-dsa no-rc2 no-des

echo Building OpenSSL...
nmake

echo Installing OpenSSL...
nmake install_sw

:done
echo.
echo === OpenSSL MSVC prebuilt ready ===
echo Location: %PREFIX%
if exist "%PREFIX%\include\openssl\ssl.h" (
    echo   include/openssl  [OK]
) else (
    echo   include/openssl  [MISSING]
)
if exist "%PREFIX%\lib\libssl.lib"   echo   lib/libssl.lib    [OK]
if exist "%PREFIX%\lib\libcrypto.lib" echo   lib/libcrypto.lib  [OK]
endlocal
