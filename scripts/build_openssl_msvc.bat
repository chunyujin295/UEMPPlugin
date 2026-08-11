@echo off
REM Build/acquire OpenSSL 3.5.7 for MSVC (x64).
REM Run from the project root in a Visual Studio x64 Native Tools Command Prompt.
REM
REM Usage: scripts\build_openssl_msvc.bat [prefix]
REM   prefix defaults to build\openssl

setlocal enabledelayedexpansion

set "PROJECT_DIR=%~dp0.."
set "SRC=%PROJECT_DIR%\3rd\source\openssl-3.5.7"

if "%~1"=="" (
    set "PREFIX=%PROJECT_DIR%\build\openssl"
) else (
    set "PREFIX=%~1"
)

echo === OpenSSL MSVC 3.5.7 ===
echo Source:  %SRC%
echo Install: %PREFIX%

REM --- Already built? ---
if exist "%PREFIX%\lib\libssl.lib" (
    echo Already installed. Remove %PREFIX% to force rebuild.
    goto :done
)

REM --- vcpkg (preferred) ---
for /f "delims=" %%i in ('where vcpkg 2^>nul') do set "VCPKG_EXE=%%i"
if defined VCPKG_EXE (
    echo vcpkg found: %VCPKG_EXE%
    vcpkg install openssl:x64-windows-static
    if !ERRORLEVEL! EQU 0 (
        for /f "tokens=*" %%d in ('vcpkg root') do set "VCPKG_ROOT=%%d"
        if exist "!VCPKG_ROOT!\installed\x64-windows-static\include\openssl" (
            xcopy /e /y "!VCPKG_ROOT!\installed\x64-windows-static\include\openssl\*" "%PREFIX%\include\openssl\" >nul
            mkdir "%PREFIX%\lib" 2>nul
            copy /y "!VCPKG_ROOT!\installed\x64-windows-static\lib\libssl.lib"   "%PREFIX%\lib\" >nul
            copy /y "!VCPKG_ROOT!\installed\x64-windows-static\lib\libcrypto.lib" "%PREFIX%\lib\" >nul
            echo Done via vcpkg.
            goto :done
        )
    )
)

REM --- Build from source ---
echo vcpkg not available, building from source...
where perl  >nul 2>&1 || (echo ERROR: perl not found in PATH & exit /b 1)
where nmake >nul 2>&1 || (echo ERROR: nmake not found - run from VS Dev Cmd Prompt & exit /b 1)

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

echo Building...
nmake

echo Installing...
nmake install_sw
echo Done via source build.

:done
echo ---
if exist "%PREFIX%\include\openssl\ssl.h"   (echo   include/openssl [OK]) else (echo   include/openssl [MISSING])
if exist "%PREFIX%\lib\libssl.lib"          (echo   lib/libssl.lib  [OK]) else (echo   lib/libssl.lib  [MISSING])
if exist "%PREFIX%\lib\libcrypto.lib"        (echo   lib/libcrypto.lib [OK]) else (echo   lib/libcrypto.lib [MISSING])
endlocal
