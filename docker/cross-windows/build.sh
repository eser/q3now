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

export PATH="$LLVM_MINGW_ROOT/bin:$PATH"

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
# native game modules ship in the zip alongside the wasm pair
cmake --build "$BUILD" --target gamecl_base gamesv_base

echo "── artifacts ──"
ls -la "$BUILD"/wired*.exe "$BUILD"/*.dll
find "$BUILD" -name '*.wasm' -maxdepth 4
echo "── test executables ──"
ls "$BUILD"/*_test.exe | wc -l
