#!/bin/bash
# Build OpenSSL 3.5.7 for MinGW-w64 and install to prebuilt directory.
# Run from the project root with Git Bash:
#   bash scripts/build_openssl_mingw.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$PROJECT_DIR/3rd/source/openssl-3.5.7"
PREFIX="$PROJECT_DIR/3rd/prebuilt/openssl/mingw"
VERSION_TARGET="3.5.7"

echo "=== OpenSSL MinGW Prebuilt ==="
echo "Target:  $PREFIX"
echo "Version: $VERSION_TARGET"

# ---- Check if already built ----
if [ -f "$PREFIX/include/openssl/opensslv.h" ]; then
    INSTALLED_VER=$(grep -oP 'OpenSSL \K\d+\.\d+\.\d+' "$PREFIX/include/openssl/opensslv.h" 2>/dev/null || echo "")
    if [ "$INSTALLED_VER" = "$VERSION_TARGET" ]; then
        echo "Already installed: OpenSSL $INSTALLED_VER"
        echo "Skipping build. Remove $PREFIX to force rebuild."
        exit 0
    fi
    echo "Version mismatch (found $INSTALLED_VER, need $VERSION_TARGET). Rebuilding..."
    rm -rf "$PREFIX"
fi

# ---- Verify tools ----
for tool in perl mingw32-make gcc; do
    if ! command -v "$tool" &>/dev/null; then
        echo "ERROR: $tool not found in PATH" >&2
        exit 1
    fi
done

# ---- Perl environment (Git's MSYS2 Perl lacks core modules) ----
source "$SCRIPT_DIR/setup_perl_env.sh"

# ---- Build ----
mkdir -p "$PREFIX"

cd "$SRC"
if [ -f Makefile ]; then
    mingw32-make clean 2>/dev/null || true
fi

echo ""
echo "=== Configuring ==="
./Configure mingw64 \
    --prefix="$PREFIX" \
    --openssldir="$PREFIX/ssl" \
    --libdir=lib \
    no-tests \
    no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 \
    no-engine no-idea no-camellia no-ssl3 \
    no-heartbeats no-gost no-deprecated \
    no-comp no-dtls no-psk no-srp no-dso no-dsa no-rc2 no-des

echo ""
echo "=== Building ($(nproc) jobs) ==="
mingw32-make -j"$(nproc)"

echo ""
echo "=== Installing ==="
mingw32-make install_sw

echo ""
echo "=== Done ==="
echo "OpenSSL $VERSION_TARGET installed to $PREFIX"
ls -la "$PREFIX/lib/libssl.a" "$PREFIX/lib/libcrypto.a"
