# Wired — Linux → Windows x86_64 cross-build image (llvm-mingw).
# The clang-based successor to the classic ioq3 mingw-gcc cross recipe:
# clang 22 + lld + compiler-rt + libunwind + mingw-w64 UCRT, no MSVC, no MSYS2.
#
#   docker build -t wired-cross-windows docker/cross-windows
#   docker run --rm -v "$PWD:/src" wired-cross-windows /src/docker/cross-windows/build.sh
#
# The image carries only the toolchain + host build tools. Dependencies
# (OpenSSL, SDL3) are cross-built by build.sh into /opt/cross-prefix inside
# the container run (cache the layer or a volume if iterating).
FROM debian:trixie-slim

ARG LLVM_MINGW_VER=20260616
# Image arch decides the toolchain host build: TARGETARCH is amd64 or arm64.
ARG TARGETARCH

# gcc + libc6-dev: HOST compiler for build-machine tools (stringify_host,
# LuaJIT's minilua/buildvm via HOST_CC) — without it FIND_PROGRAM falls
# through to llvm-mingw's bare clang, which has no Linux sysroot.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils git \
        cmake ninja-build make perl pkg-config \
        gcc libc6-dev nasm python3 golang \
    && rm -rf /var/lib/apt/lists/*

RUN set -e; \
    case "$TARGETARCH" in \
      arm64) HOSTTAG=ubuntu-22.04-aarch64 ;; \
      *)     HOSTTAG=ubuntu-22.04-x86_64 ;; \
    esac; \
    curl -sL "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VER}/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${HOSTTAG}.tar.xz" \
      | tar xJ -C /opt; \
    ln -s "/opt/llvm-mingw-${LLVM_MINGW_VER}-ucrt-${HOSTTAG}" /opt/llvm-mingw; \
    printf '#!/bin/sh\nexec "$(dirname "$0")/x86_64-w64-mingw32-windres" "$@"\n' > /opt/llvm-mingw/bin/windres; \
    chmod +x /opt/llvm-mingw/bin/windres

# wasi-sdk: game modules (gamecl/gamesv.wasm) are built with the wasi
# toolchain regardless of the engine target. /opt/wasi-sdk is one of
# wasm_tools.cmake's auto-detect candidates, so no env var is needed.
# Version pin mirrors .wasi-sdk-version at the repo root.
ARG WASI_SDK_VER=33.0
RUN set -e; \
    case "$TARGETARCH" in \
      arm64) WASIARCH=arm64 ;; \
      *)     WASIARCH=x86_64 ;; \
    esac; \
    curl -sL "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VER%%.*}/wasi-sdk-${WASI_SDK_VER}-${WASIARCH}-linux.tar.gz" \
      | tar xz -C /opt; \
    ln -s "/opt/wasi-sdk-${WASI_SDK_VER}-${WASIARCH}-linux" /opt/wasi-sdk

ENV LLVM_MINGW_ROOT=/opt/llvm-mingw \
    PATH=/opt/llvm-mingw/bin:$PATH

# ── cross-built dependencies, baked as image layers ──────────────────────────
# OpenSSL + SDL3 change only when the pins below change; with buildx + GHA
# layer cache every CI run gets them for free instead of ~3-4 min of rebuild.
ARG OPENSSL_VER=3.5.4
RUN set -e; cd /tmp; \
    curl -sLO "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VER}/openssl-${OPENSSL_VER}.tar.gz"; \
    tar xf "openssl-${OPENSSL_VER}.tar.gz"; cd "openssl-${OPENSSL_VER}"; \
    ./Configure mingw64 no-shared no-tests no-apps \
        --prefix=/opt/cross-prefix --cross-compile-prefix=x86_64-w64-mingw32- >/dev/null; \
    make -j"$(nproc)" >/dev/null 2>&1; make install_sw >/dev/null 2>&1; \
    rm -rf /tmp/openssl-*

ARG SDL_TAG=release-3.4.14
RUN set -e; \
    git clone -q --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git /tmp/SDL; \
    cmake -S /tmp/SDL -B /tmp/SDL/build -G Ninja \
        -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
        -DCMAKE_C_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang \
        -DCMAKE_CXX_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang++ \
        -DCMAKE_RC_COMPILER=/opt/llvm-mingw/bin/windres \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/cross-prefix \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL_TESTS=OFF >/dev/null; \
    cmake --build /tmp/SDL/build --parallel >/dev/null; \
    cmake --install /tmp/SDL/build >/dev/null; \
    rm -rf /tmp/SDL
