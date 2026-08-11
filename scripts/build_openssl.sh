#!/bin/bash
# Build OpenSSL from vendored source for Linux.
# Follows official INSTALL.md: ./Configure → make → make install_sw
#
# Usage: bash scripts/build_openssl.sh [prefix]
#        Default prefix: build/openssl

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$PROJECT_DIR/3rd/source/openssl-3.5.7"
PREFIX="${1:-$PROJECT_DIR/build/openssl}"

echo "=== OpenSSL 3.5.7 (linux) ==="
echo "Prefix: $PREFIX"

if [ -f "$PREFIX/lib/libssl.a" ]; then
    echo "Already installed. Remove $PREFIX to rebuild."
    exit 0
fi

cd "$SRC"

./Configure \
    --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib \
    no-tests no-demos \
    no-cast no-md2 no-md4 no-mdc2 no-rc4 no-rc5 \
    no-engine no-idea no-camellia no-ssl3 \
    no-heartbeats no-gost no-deprecated \
    no-comp no-dtls no-psk no-srp no-dso no-dsa no-rc2 no-des

make -j"$(nproc)"
make install_sw

echo "=== Done ==="
ls -la "$PREFIX/lib/libssl.a" "$PREFIX/lib/libcrypto.a"
