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

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils git \
        cmake ninja-build make perl pkg-config \
        nasm python3 golang \
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

ENV LLVM_MINGW_ROOT=/opt/llvm-mingw \
    PATH=/opt/llvm-mingw/bin:$PATH
