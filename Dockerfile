# ══════════════════════════════════════════════════════════════════════════════
# q3now dedicated server — multi-stage Docker build
# ══════════════════════════════════════════════════════════════════════════════
#
# Usage:
#   docker build -t q3now-server .
#   docker run -p 27960:27960/udp \
#     -v ./baseq3:/home/q3now/baseq3 \
#     eserozvataf/q3now +map q3dm17
#
# One UDP port serves all clients — QUIC (WebTransport) and legacy Q3 protocol
# share port 27960/udp via in-engine packet demultiplexing.
# Game assets (pak0.pk3, custom maps, server.cfg) go in the mounted volume.
# See docker/docker-compose.yml for a complete example.
# ══════════════════════════════════════════════════════════════════════════════

# ── Stage 1: Builder ────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS builder

ARG WASI_SDK_VERSION=32

# libx11-dev: required at cmake configure time — the CMakeLists.txt always
# processes client target definitions which call find_package(X11 REQUIRED),
# even though the dedicated server itself never links against X11.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        ca-certificates \
        curl \
        libcurl4-openssl-dev \
        libssl-dev \
        libx11-dev \
    && rm -rf /var/lib/apt/lists/*

# Install wasi-sdk for WASM game module compilation
# Detect host architecture for correct wasi-sdk variant
RUN ARCH=$(uname -m) && \
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

# Configure: dedicated server only (no SDL, no renderers)
# WASM enabled for portable game module support
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_SDL=OFF \
    -DUSE_OPENGL=OFF \
    -DUSE_VULKAN=OFF \
    -DUSE_RENDERER_DLOPEN=OFF \
    -DUSE_WASM=ON \
    -DWASI_SDK_PREFIX=/opt/wasi-sdk

# Build only the dedicated server + game modules (skip client which needs
# additional graphics/audio deps we don't have). Copy binary to a fixed path
# so the runtime stage doesn't need to know the architecture suffix.
RUN ARCH=$(uname -m) && \
    case "$ARCH" in \
      x86_64)  BINEXT=".x86_64" ;; \
      aarch64) BINEXT=".aarch64" ;; \
      armv7l)  BINEXT=".arm" ;; \
      *)       BINEXT="" ;; \
    esac && \
    cmake --build build --target "q3now-ded${BINEXT}" \
      cgame_baseq3 qagame_baseq3 ui_baseq3 \
      qagame_wasm cgame_wasm ui_wasm \
      --parallel $(nproc) && \
    cp "build/q3now-ded${BINEXT}" /tmp/q3now-ded

# ── Stage 2: Runtime ────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libcurl4 \
        libssl3 \
        openssl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd -g 1000 q3now \
    && useradd -u 1000 -g q3now -m -d /home/q3now -s /bin/sh q3now

# Install dedicated server binary (arch-independent path)
COPY --from=builder /tmp/q3now-ded /opt/q3now/q3now-ded

# Install game modules (native .so + WASM .wasm)
COPY --from=builder /src/build/Release/baseq3/ /opt/q3now/baseq3/

# Install default server config
COPY baseq3/q3config_server.cfg /opt/q3now/baseq3/q3config_server.cfg

# Install entrypoint
COPY docker/entrypoint.sh /opt/q3now/entrypoint.sh
RUN chmod +x /opt/q3now/entrypoint.sh /opt/q3now/q3now-ded

# Create writable homepath directory for volume mounts
# Operators mount game assets (pak files, configs) at /home/q3now/baseq3
RUN mkdir -p /home/q3now/baseq3 /home/q3now/certs \
    && chown -R q3now:q3now /home/q3now

# Single UDP port for all traffic — QUIC and the legacy Q3 protocol share
# the same socket; the engine demultiplexes on the first bytes of each packet.
EXPOSE 27960/udp

USER q3now
WORKDIR /opt/q3now

ENTRYPOINT ["/opt/q3now/entrypoint.sh"]
CMD ["+map", "q3dm17"]
