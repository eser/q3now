# ══════════════════════════════════════════════════════════════════════════════
# q3now headless server — multi-stage Docker build
# ══════════════════════════════════════════════════════════════════════════════
#
# Usage:
#   docker build -t q3now-server .
#   docker run -p 27960:27960/udp \
#     -v ./base:/home/wired/base \
#     eserozvataf/q3now +map arena7
#
# One UDP port serves all clients — QUIC (WebTransport) and legacy Q3 protocol
# share port 27960/udp via in-engine packet demultiplexing.
# Game assets (pak0.pk3, custom maps, server.cfg) go in the mounted volume.
# See docker/docker-compose.yml for a complete example.
# ══════════════════════════════════════════════════════════════════════════════

# ── Stage 1: Builder ────────────────────────────────────────────────────────
FROM debian:trixie-slim AS builder

# wasi-sdk version is single-sourced from .wasi-sdk-version at the repo root —
# CI, this Dockerfile and the docs all read the same pin (they had drifted:
# this file and CI said 32 while the docs pinned 33).
COPY .wasi-sdk-version /tmp/wasi-sdk-version

# libsdl3-dev: required at cmake configure time — the CMakeLists.txt always
# processes the client/window-system target definitions which call
# find_package(SDL3 REQUIRED), even though the headless server itself never
# links against the SDL window system.
# clang: single-toolchain policy (2026-08-14) — every build lane compiles
# with clang; trixie ships clang 19, close enough for this container check.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential clang \
        cmake \
        ninja-build \
        ca-certificates \
        curl \
        git \
        golang-go \
        libssl-dev \
        libsdl3-dev \
    && rm -rf /var/lib/apt/lists/*
# golang-go builds tools/sw3z-archiver, which packs pax21.sw3z. Without the pak
# the image has no default.cfg and the server refuses to start — see the pak
# build step below.

# Install wasi-sdk for WASM game module compilation
# Detect host architecture for correct wasi-sdk variant
RUN WASI_SDK_VERSION=$(cat /tmp/wasi-sdk-version) && \
    ARCH=$(uname -m) && \
    case "$ARCH" in \
      x86_64)  WASI_ARCH="x86_64" ;; \
      aarch64) WASI_ARCH="arm64" ;; \
      *)       WASI_ARCH="$ARCH" ;; \
    esac && \
    curl -sL "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VERSION}/wasi-sdk-${WASI_SDK_VERSION}.0-${WASI_ARCH}-linux.tar.gz" \
    | tar xz -C /opt \
    && ln -sf /opt/wasi-sdk-${WASI_SDK_VERSION}.0-*-linux /opt/wasi-sdk

WORKDIR /src
COPY . .

# Configure: headless server only (no renderers). The window system is SDL3
# (configure-time dependency only; the headless target does not link it).
# WASM enabled for portable game module support.
RUN CC=clang CXX=clang++ cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_OPENGL=OFF \
    -DUSE_VULKAN=OFF \
    -DUSE_RENDERER_DLOPEN=OFF \
    -DUSE_WASM=ON \
    -DWASI_SDK_PREFIX=/opt/wasi-sdk

# Build only the headless server + game modules (skip client which needs
# additional graphics/audio deps we don't have). Copy binary to a fixed path
# so the runtime stage doesn't need to know the architecture suffix.
RUN ARCH=$(uname -m) && \
    case "$ARCH" in \
      x86_64)  BINEXT=".x86_64" ;; \
      aarch64) BINEXT=".aarch64" ;; \
      riscv64) BINEXT=".riscv64" ;; \
      armv7l)  BINEXT=".arm" ;; \
      *)       BINEXT="" ;; \
    esac && \
    cmake --build build --target "wired-headless${BINEXT}" \
      gamecl_base gamesv_base \
      gamesv_wasm gamecl_wasm \
      --parallel $(nproc) && \
    cp "build/wired-headless${BINEXT}" /tmp/wired-headless

# Pack pax21.sw3z — default.cfg, scripts and the VM modules.
#
# This step used to be missing, and the omission was invisible: the runtime stage
# copies build/Release/base/, CMake never creates that directory, and COPY of a
# non-existent directory is not an error. The image built and published happily,
# then every `docker run` died with "Couldn't load default.cfg" (files.c) — a
# message that reads like missing game data rather than a broken image.
#
# The pak is a Makefile target, not a CMake one, because packing runs through the
# Go archiver in tools/sw3z-archiver. BUILD_DIR must match what the cmake step
# above used, so the Makefile writes the pak where the runtime stage looks for it.
#
# The pak is packed by the Go archiver in tools/sw3z-archiver, driven from the
# Makefile — there is no CMake target for it, which is why the cmake step above
# cannot produce it and why this step exists at all.
#
# `create-packs` is NOT used, even though it is the named entry point: it depends
# on the `build` target, which configures and compiles the entire tree including
# the CTest executables. This image needs none of them and cannot link them —
# the first attempt died on vk_temporal_main_activation_test. Naming the pak file
# target instead reaches the same rule while skipping the engine graph; its own
# prerequisites are just the WASM modules the cmake step already built, modfiles/
# and the archiver, so it still repacks whenever those change.
#
# The runtime stage copies build/Release/base/, which is where MODULE_DIR
# (=$(BUILD_DIR)/$(BUILD_CFG)/base) puts things for this Release configuration;
# the pak lands one level up in build/base/, hence the explicit copy.
#
# The test is the point: it converts "the pak silently did not appear" into a
# build failure here rather than a runtime failure in the operator's terminal.
# The original omission was invisible precisely because nothing checked — COPY of
# a non-existent directory is not an error in Docker, so the image published fine
# and every run then died with "Couldn't load default.cfg".
RUN make build/base/pax21.sw3z BUILD_DIR=build DEV=0 \
    && test -f build/base/pax21.sw3z \
    && mkdir -p build/Release/base \
    && cp build/base/pax21.sw3z build/Release/base/

# ── Stage 2: Runtime ────────────────────────────────────────────────────────
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 \
        openssl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd -g 1000 wired \
    && useradd -u 1000 -g wired -m -d /home/wired -s /bin/sh wired

# Install headless server binary (arch-independent path)
COPY --from=builder /tmp/wired-headless /opt/wired/wired-headless

# Install game modules (native .so + WASM .wasm)
COPY --from=builder /src/build/Release/base/ /opt/wired/base/

# Install default server config
COPY modfiles/config_server.cfg /opt/wired/base/config_server.cfg

# Ship the engine and bundled third-party notices with the runtime artifact.
COPY --from=builder /src/LICENSE /opt/wired/LICENSE
COPY --from=builder /src/THIRD_PARTY_LICENSES.md /opt/wired/THIRD_PARTY_LICENSES.md
COPY --from=builder /src/LICENSES/ /opt/wired/LICENSES/

# Install entrypoint
COPY docker/entrypoint.sh /opt/wired/entrypoint.sh
RUN chmod +x /opt/wired/entrypoint.sh /opt/wired/wired-headless

# Create writable homepath directory for volume mounts
# Operators mount game assets (pak files, configs) at /home/wired/base
RUN mkdir -p /home/wired/base /home/wired/certs \
    && chown -R wired:wired /home/wired

# Single UDP port for all traffic — QUIC and the legacy Q3 protocol share
# the same socket; the engine demultiplexes on the first bytes of each packet.
EXPOSE 27960/udp

USER wired
WORKDIR /opt/wired

ENTRYPOINT ["/opt/wired/entrypoint.sh"]
CMD ["+map", "arena7"]
