#!/bin/bash
# Wired — in-container Linux → Windows x86_64 cross build (llvm-mingw).
# Run inside the wired-cross-windows image with the repo mounted at /src:
#   docker run --rm -v "$PWD:/src" wired-cross-windows /src/docker/cross-windows/build.sh
#
# Produces build/cross-windows-docker/ inside the mount: wired.x64.exe,
# wired-headless.x64.exe, renderer DLLs, gamecl/gamesv.wasm.
#
set -euo pipefail

SRC=/src
BUILD="$SRC/build/cross-windows-docker"
PREFIX=/opt/cross-prefix
JOBS=$(nproc)

OPENSSL_VER=3.5.4
SDL_TAG=release-3.4.14

export PATH="$LLVM_MINGW_ROOT/bin:$PATH"

# ── deps: OpenSSL (static) ───────────────────────────────────────────────────
if [ ! -f "$PREFIX/lib/libcrypto.a" ] && [ ! -f "$PREFIX/lib64/libcrypto.a" ]; then
    mkdir -p /tmp/deps && cd /tmp/deps
    curl -sLO "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VER}/openssl-${OPENSSL_VER}.tar.gz"
    tar xf "openssl-${OPENSSL_VER}.tar.gz" && cd "openssl-${OPENSSL_VER}"
    ./Configure mingw64 no-shared no-tests no-apps \
        --prefix="$PREFIX" --cross-compile-prefix=x86_64-w64-mingw32- >/dev/null
    make -j"$JOBS" >/dev/null 2>&1
    make install_sw >/dev/null 2>&1
fi

# ── deps: SDL3 (static, project pin) ─────────────────────────────────────────
if [ ! -f "$PREFIX/lib/libSDL3.a" ]; then
    cd /tmp/deps 2>/dev/null || { mkdir -p /tmp/deps && cd /tmp/deps; }
    [ -d SDL ] || git clone -q --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git
    cmake -S SDL -B SDL/build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/toolchains/llvm-mingw-x86_64.cmake" \
        -DLLVM_MINGW_ROOT="$LLVM_MINGW_ROOT" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON \
        -DSDL_INSTALL_TESTS=OFF >/dev/null
    cmake --build SDL/build --parallel >/dev/null
    cmake --install SDL/build >/dev/null
fi

# ── engine configure ─────────────────────────────────────────────────────────
cd "$SRC"
cmake -S . -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-x86_64.cmake \
    -DLLVM_MINGW_ROOT="$LLVM_MINGW_ROOT" \
    -DWIRED_CROSS_PREFIX="$PREFIX" \
    -DCMAKE_INSTALL_PREFIX="$BUILD/stage" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    -DUSE_WASM=ON

# ── engine build ─────────────────────────────────────────────────────────────
cmake --build "$BUILD" --parallel

echo "── artifacts ──"
ls -la "$BUILD"/wired*.exe "$BUILD"/*.dll
find "$BUILD" -name '*.wasm' -maxdepth 4
echo "── test executables ──"
ls "$BUILD"/*_test.exe | wc -l
